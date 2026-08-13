// Copyright 2026 Jinwoo Sung
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Reference driver for a serial device that emits one reading per line, e.g.
//
//     T=23.5\n
//
// It is the shape `docs/architecture.md` prescribes for a real driver, and it
// is deliberately the *direct* path rather than a raw byte bridge: the line is
// parsed inside the Wirestead callback and only the finished
// sensor_msgs/Temperature is published, so the payload never takes an extra
// DDS hop just to be parsed on the other side.
//
// Four things here are the point, and are what a driver author should copy:
//
//   1. Parsing happens on the callback-scoped view. No copy is taken, because
//      the parse finishes before the callback returns.
//   2. CallbackGate closes before the channel stops, so no callback can touch
//      a publisher that on_deactivate is about to tear down.
//   3. The message is stamped from ctx.received_at() rather than from the
//      clock at publish time, so queueing and parsing do not leak into the
//      timestamp.
//   4. The link's counters are published on /diagnostics through the same
//      helper every driver should use.

#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <diagnostic_updater/diagnostic_updater.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <sensor_msgs/msg/temperature.hpp>

#include "wirestead/builder/serial_builder.hpp"
#include "wirestead_ros/callback_gate.hpp"
#include "wirestead_ros/diagnostics.hpp"

namespace wirestead_ros_examples
{

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class SerialLineDriver : public rclcpp_lifecycle::LifecycleNode
{
public:
  explicit SerialLineDriver(const rclcpp::NodeOptions & options)
  : rclcpp_lifecycle::LifecycleNode("serial_line_driver", options)
  {
    // Endpoint and framing are immutable while active: changing them needs a
    // deactivate/configure/activate cycle, not a live parameter write.
    declare_parameter<std::string>("device", "/dev/ttyUSB0");
    declare_parameter<int>("baud_rate", 115200);
    declare_parameter<std::string>("frame_id", "sensor_link");
  }

  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override
  {
    frame_id_ = get_parameter("frame_id").as_string();
    const auto device = get_parameter("device").as_string();
    const auto baud = static_cast<uint32_t>(get_parameter("baud_rate").as_int());

    // Sensor-data QoS: best effort, keep last. A late reading is worth less
    // than a fresh one, which is also why the transport side is not asked to
    // be reliable either.
    publisher_ = create_publisher<sensor_msgs::msg::Temperature>(
      "temperature", rclcpp::SensorDataQoS());

    updater_ = std::make_unique<diagnostic_updater::Updater>(this);
    updater_->setHardwareID(device);
    updater_->add("serial link", [this](diagnostic_updater::DiagnosticStatusWrapper & status) {
        if (!channel_) {
          status.summary(diagnostic_msgs::msg::DiagnosticStatus::STALE, "not configured");
          return;
        }
        wirestead_ros::report_channel_stats(status, channel_->stats(), channel_->connected());
      });

    channel_ = wirestead::builder::SerialBuilder(device, baud)
      .use_line_framer("\n")
      .on_message([this](const wirestead::wrapper::MessageContext & ctx) {on_line(ctx);})
      .build();

    RCLCPP_INFO(get_logger(), "configured for %s at %u baud", device.c_str(), baud);
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_activate(const rclcpp_lifecycle::State & state) override
  {
    LifecycleNode::on_activate(state);
    // Publisher first, then admit callbacks, then start the transport. The
    // reverse order would let a line arrive before there is anywhere to put it.
    gate_.open();

    auto started = channel_->start();
    if (started.wait_for(std::chrono::seconds(2)) != std::future_status::ready || !started.get()) {
      // A serial port that is not there yet is not an activation failure: the
      // transport's own reopen loop keeps trying, and lifecycle state is not
      // connection state. Only a configuration error should fail activation.
      RCLCPP_WARN(get_logger(), "port not open yet; the reopen loop will keep trying");
    }
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_deactivate(const rclcpp_lifecycle::State & state) override
  {
    // The order in docs/architecture.md, and the reason CallbackGate exists:
    // close first so no new line is admitted, wait for the ones in flight,
    // and only then take away what they publish through.
    gate_.close_and_wait();
    if (channel_) {channel_->stop();}
    LifecycleNode::on_deactivate(state);
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override
  {
    channel_.reset();
    updater_.reset();
    publisher_.reset();
    return CallbackReturn::SUCCESS;
  }

  CallbackReturn on_shutdown(const rclcpp_lifecycle::State & state) override
  {
    gate_.close_and_wait();
    if (channel_) {channel_->stop();}
    LifecycleNode::on_deactivate(state);
    channel_.reset();
    return CallbackReturn::SUCCESS;
  }

private:
  // Runs on a Wirestead io thread, not the ROS executor.
  void on_line(const wirestead::wrapper::MessageContext & ctx)
  {
    auto lease = gate_.try_enter();
    if (!lease) {return;}  // deactivating: the publisher may already be gone

    // ctx.data() is valid only until this returns, which is fine because the
    // parse finishes here. A driver that hands the payload to another thread
    // would have to call ctx.data_as_vector() instead.
    const auto line = ctx.data();
    if (line.rfind("T=", 0) != 0) {return;}

    double celsius = 0.0;
    try {
      celsius = std::stod(std::string(line.substr(2)));
    } catch (const std::exception &) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 5000, "unparseable line: %s", std::string(line).c_str());
      return;
    }

    sensor_msgs::msg::Temperature msg;
    msg.header.frame_id = frame_id_;
    // Stamped from when the bytes arrived, not from now: on a burst the
    // difference is every line after the first sharing one timestamp.
    const auto age = std::chrono::steady_clock::now() - ctx.received_at();
    msg.header.stamp = now() - rclcpp::Duration(age);
    msg.temperature = celsius;
    msg.variance = 0.0;

    publisher_->publish(msg);
  }

  std::string frame_id_;
  wirestead_ros::CallbackGate gate_;
  std::unique_ptr<wirestead::wrapper::Serial> channel_;
  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Temperature>::SharedPtr publisher_;
  std::unique_ptr<diagnostic_updater::Updater> updater_;
};

}  // namespace wirestead_ros_examples
