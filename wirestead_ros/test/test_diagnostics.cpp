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

#include <chrono>
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
// The failure this whole field exists for: the link is up, every counter looks
// healthy, and the device simply stopped talking.
TEST(DiagnosticsTest, SilenceBeyondTheThresholdIsStale)
{
  diagnostic_updater::DiagnosticStatusWrapper status;
  wirestead::wrapper::RuntimeStats stats;
  stats.messages_received = 1000;
  stats.last_receive_age_ms = 900;

  wirestead_ros::report_channel_stats(status, stats, true, std::chrono::milliseconds(500));

  EXPECT_EQ(status.level, DiagnosticStatus::STALE);
  EXPECT_EQ(value_of(status, "connected"), "True");
  EXPECT_EQ(value_of(status, "last_receive_age_ms"), "900");
}

TEST(DiagnosticsTest, FreshDataWithinTheThresholdIsOk)
{
  diagnostic_updater::DiagnosticStatusWrapper status;
  wirestead::wrapper::RuntimeStats stats;
  stats.last_receive_age_ms = 100;

  wirestead_ros::report_channel_stats(status, stats, true, std::chrono::milliseconds(500));

  EXPECT_EQ(status.level, DiagnosticStatus::OK);
}

// Without a threshold the caller has not said what "too long" means, so
// silence is reported as a value and nothing more.
TEST(DiagnosticsTest, SilenceDoesNotChangeTheLevelWithoutAThreshold)
{
  diagnostic_updater::DiagnosticStatusWrapper status;
  wirestead::wrapper::RuntimeStats stats;
  stats.last_receive_age_ms = 60000;

  wirestead_ros::report_channel_stats(status, stats, true);

  EXPECT_EQ(status.level, DiagnosticStatus::OK);
  EXPECT_EQ(value_of(status, "last_receive_age_ms"), "60000");
}

// A link that has never received anything is starting up, not stalled.
// Reporting STALE from the first tick would cry wolf on every launch.
TEST(DiagnosticsTest, AChannelThatHasNeverReceivedIsNotStale)
{
  diagnostic_updater::DiagnosticStatusWrapper status;
  wirestead::wrapper::RuntimeStats stats;  // last_receive_age_ms stays unset

  wirestead_ros::report_channel_stats(status, stats, true, std::chrono::milliseconds(1));

  EXPECT_EQ(status.level, DiagnosticStatus::OK);
  EXPECT_EQ(value_of(status, "last_receive_age_ms"), "never");
}

// Disconnected outranks stale: there is no point reporting an old reading when
// the link itself is down.
TEST(DiagnosticsTest, DisconnectedOutranksStale)
{
  diagnostic_updater::DiagnosticStatusWrapper status;
  wirestead::wrapper::RuntimeStats stats;
  stats.last_receive_age_ms = 90000;

  wirestead_ros::report_channel_stats(status, stats, false, std::chrono::milliseconds(500));

  EXPECT_EQ(status.level, DiagnosticStatus::ERROR);
}

// A silent sensor is the more urgent fact than a full queue, and it is the one
// every other field reports as healthy.
TEST(DiagnosticsTest, StaleOutranksBackpressure)
{
  diagnostic_updater::DiagnosticStatusWrapper status;
  wirestead::wrapper::RuntimeStats stats;
  stats.backpressure_active = true;
  stats.last_receive_age_ms = 5000;

  wirestead_ros::report_channel_stats(status, stats, true, std::chrono::milliseconds(500));

  EXPECT_EQ(status.level, DiagnosticStatus::STALE);
}

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
