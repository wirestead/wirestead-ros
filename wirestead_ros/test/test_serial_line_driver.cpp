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
// Drives the reference driver over a pseudo-terminal. This is the test
// docs/ros2_support_analysis.md asks for before claiming the lifecycle and
// callback contracts hold: compiling the example proves only that it compiles.

#include <gtest/gtest.h>

#include <pty.h>
#include <unistd.h>

#include <chrono>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/temperature.hpp>

#include "serial_line_driver.hpp"

using namespace std::chrono_literals;
using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

namespace
{

// A pseudo-terminal pair stands in for the device: the driver opens the slave
// exactly as it would open /dev/ttyUSB0, and the test writes lines into the
// master.
class PtyDevice
{
public:
  PtyDevice()
  {
    char name[256];
    if (::openpty(&master_, &slave_, name, nullptr, nullptr) == 0) {
      path_ = name;
      ok_ = true;
    }
  }

  ~PtyDevice()
  {
    if (master_ >= 0) {::close(master_);}
    if (slave_ >= 0) {::close(slave_);}
  }

  bool ok() const {return ok_;}
  const std::string & path() const {return path_;}

  void write_line(const std::string & line)
  {
    const std::string payload = line + "\n";
    ssize_t written = ::write(master_, payload.data(), payload.size());
    (void)written;
  }

private:
  int master_{-1};
  int slave_{-1};
  std::string path_;
  bool ok_{false};
};

rclcpp::NodeOptions options_for(const std::string & device)
{
  rclcpp::NodeOptions options;
  options.parameter_overrides(
    {
      rclcpp::Parameter("device", device),
      rclcpp::Parameter("baud_rate", 115200),
      rclcpp::Parameter("frame_id", "test_link"),
    });
  return options;
}

// Spins the node until `pred` holds or the deadline passes. Returns whether it
// held, so the caller asserts rather than sleeping a fixed amount.
template<typename Pred>
bool spin_until(
  const std::shared_ptr<wirestead_ros_examples::SerialLineDriver> & node,
  rclcpp::Node::SharedPtr helper, Pred pred, std::chrono::milliseconds timeout)
{
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node->get_node_base_interface());
  exec.add_node(helper);
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) {return true;}
    exec.spin_some(10ms);
  }
  return pred();
}

class SerialLineDriverTest : public ::testing::Test
{
protected:
  static void SetUpTestSuite()
  {
    if (!rclcpp::ok()) {rclcpp::init(0, nullptr);}
  }
};

}  // namespace

// The whole point of the driver: a line on the wire becomes one semantic
// message, with the value parsed out and the configured frame attached.
TEST_F(SerialLineDriverTest, PublishesATemperatureForEachLine)
{
  PtyDevice pty;
  if (!pty.ok()) {
    GTEST_SKIP() << "could not allocate a pseudo-terminal";
  }

  auto node = std::make_shared<wirestead_ros_examples::SerialLineDriver>(options_for(pty.path()));
  ASSERT_EQ(node->on_configure(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
  ASSERT_EQ(node->on_activate(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);

  auto listener = std::make_shared<rclcpp::Node>("temperature_listener");
  sensor_msgs::msg::Temperature received;
  bool got_one = false;
  auto sub = listener->create_subscription<sensor_msgs::msg::Temperature>(
    "/temperature", rclcpp::SensorDataQoS(),
    [&](sensor_msgs::msg::Temperature::SharedPtr msg) {
      received = *msg;
      got_one = true;
    });

  pty.write_line("T=23.5");

  EXPECT_TRUE(spin_until(node, listener, [&] {return got_one;}, 5s))
    << "a line reached the driver but no Temperature was published";
  if (got_one) {
    EXPECT_NEAR(received.temperature, 23.5, 1e-9);
    EXPECT_EQ(received.header.frame_id, "test_link");
    EXPECT_GT(received.header.stamp.sec, 0) << "the message was published with an unset stamp";
  }

  EXPECT_EQ(node->on_deactivate(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
  EXPECT_EQ(node->on_cleanup(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
}

// A line that does not match the protocol must be dropped rather than
// published as garbage or crashing the io thread it is parsed on.
TEST_F(SerialLineDriverTest, IgnoresLinesThatAreNotReadings)
{
  PtyDevice pty;
  if (!pty.ok()) {
    GTEST_SKIP() << "could not allocate a pseudo-terminal";
  }

  auto node = std::make_shared<wirestead_ros_examples::SerialLineDriver>(options_for(pty.path()));
  ASSERT_EQ(node->on_configure(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
  ASSERT_EQ(node->on_activate(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);

  auto listener = std::make_shared<rclcpp::Node>("reject_listener");
  int published = 0;
  auto sub = listener->create_subscription<sensor_msgs::msg::Temperature>(
    "/temperature", rclcpp::SensorDataQoS(),
    [&](sensor_msgs::msg::Temperature::SharedPtr) {published++;});

  pty.write_line("BOOT ok");
  pty.write_line("T=not-a-number");
  pty.write_line("");

  // Then a good line, so the test waits on a real event rather than on time
  // passing - if the driver had died on the bad input this never arrives.
  pty.write_line("T=1.25");

  EXPECT_TRUE(spin_until(node, listener, [&] {return published > 0;}, 5s))
    << "the driver stopped publishing after unparseable input";
  EXPECT_EQ(published, 1) << "an unparseable line was published as a reading";

  EXPECT_EQ(node->on_deactivate(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
  EXPECT_EQ(node->on_cleanup(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
}

// Documented contract: lifecycle state is not connection state. A device that
// is not plugged in yet must leave the node active with its reopen loop
// running, not fail activation.
TEST_F(SerialLineDriverTest, ActivatesWithoutTheDevicePresent)
{
  auto node = std::make_shared<wirestead_ros_examples::SerialLineDriver>(
    options_for("/dev/wirestead-does-not-exist"));

  ASSERT_EQ(node->on_configure(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
  EXPECT_EQ(node->on_activate(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
  EXPECT_EQ(node->on_deactivate(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
  EXPECT_EQ(node->on_cleanup(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
}

// Repeated cycles are where a shutdown-ordering mistake shows up: the second
// activate reuses state the first deactivate was supposed to have released.
TEST_F(SerialLineDriverTest, SurvivesRepeatedActivateDeactivateCycles)
{
  PtyDevice pty;
  if (!pty.ok()) {
    GTEST_SKIP() << "could not allocate a pseudo-terminal";
  }

  auto node = std::make_shared<wirestead_ros_examples::SerialLineDriver>(options_for(pty.path()));
  ASSERT_EQ(node->on_configure(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);

  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(node->on_activate(rclcpp_lifecycle::State()),
      CallbackReturn::SUCCESS) << "cycle " << i;
    pty.write_line("T=7.0");
    ASSERT_EQ(node->on_deactivate(rclcpp_lifecycle::State()),
      CallbackReturn::SUCCESS) << "cycle " << i;
  }

  EXPECT_EQ(node->on_cleanup(rclcpp_lifecycle::State()), CallbackReturn::SUCCESS);
}
