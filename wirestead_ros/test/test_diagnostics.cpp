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

#include <gtest/gtest.h>

#include <string>

#include <diagnostic_msgs/msg/diagnostic_status.hpp>

#include "wirestead_ros/diagnostics.hpp"

namespace
{

using diagnostic_msgs::msg::DiagnosticStatus;

std::string value_of(
  const diagnostic_updater::DiagnosticStatusWrapper & status, const std::string & key)
{
  for (const auto & kv : status.values) {
    if (kv.key == key) {
      return kv.value;
    }
  }
  return "<missing>";
}

}  // namespace

TEST(DiagnosticsTest, DisconnectedIsAnError)
{
  diagnostic_updater::DiagnosticStatusWrapper status;
  wirestead::wrapper::RuntimeStats stats;

  wirestead_ros::report_channel_stats(status, stats, false);

  EXPECT_EQ(status.level, DiagnosticStatus::ERROR);
  EXPECT_EQ(value_of(status, "connected"), "False");
}

TEST(DiagnosticsTest, ConnectedAndIdleIsOk)
{
  diagnostic_updater::DiagnosticStatusWrapper status;
  wirestead::wrapper::RuntimeStats stats;
  stats.messages_received = 42;

  wirestead_ros::report_channel_stats(status, stats, true);

  EXPECT_EQ(status.level, DiagnosticStatus::OK);
  EXPECT_EQ(value_of(status, "messages_received"), "42");
}

TEST(DiagnosticsTest, ActiveBackpressureWarns)
{
  diagnostic_updater::DiagnosticStatusWrapper status;
  wirestead::wrapper::RuntimeStats stats;
  stats.backpressure_active = true;

  wirestead_ros::report_channel_stats(status, stats, true);

  EXPECT_EQ(status.level, DiagnosticStatus::WARN);
}

// The counters are cumulative since the channel started, so raising the level
// on a non-zero total would latch a warning for the rest of the run over a
// single drop that has long since stopped. A caller that wants "dropping now"
// diffs successive samples and amends the level itself.
TEST(DiagnosticsTest, CumulativeDropsDoNotLatchTheLevel)
{
  diagnostic_updater::DiagnosticStatusWrapper status;
  wirestead::wrapper::RuntimeStats stats;
  stats.dropped_messages = 17;
  stats.dropped_bytes = 4096;
  stats.failed_sends = 3;
  stats.backpressure_events = 9;

  wirestead_ros::report_channel_stats(status, stats, true);

  EXPECT_EQ(status.level, DiagnosticStatus::OK);
  EXPECT_EQ(value_of(status, "dropped_messages"), "17");
  EXPECT_EQ(value_of(status, "dropped_bytes"), "4096");
  EXPECT_EQ(value_of(status, "failed_sends"), "3");
  EXPECT_EQ(value_of(status, "backpressure_events"), "9");
}
