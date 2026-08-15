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

#include "wirestead_ros/diagnostics.hpp"

#include <string>

#include <diagnostic_msgs/msg/diagnostic_status.hpp>

namespace wirestead_ros
{

void report_channel_stats(
  diagnostic_updater::DiagnosticStatusWrapper & status,
  const wirestead::wrapper::RuntimeStats & stats,
  bool connected,
  std::optional<std::chrono::milliseconds> stale_after)
{
  using diagnostic_msgs::msg::DiagnosticStatus;

  // Silence counts as stale only once something has arrived: a link that has
  // never received is starting up, not stalled, and reporting STALE from the
  // first tick would cry wolf on every launch.
  const bool stale = stale_after && stats.last_receive_age_ms &&
    *stats.last_receive_age_ms > static_cast<uint64_t>(stale_after->count());

  if (!connected) {
    status.summary(DiagnosticStatus::ERROR, "disconnected");
  } else if (stale) {
    // Ahead of backpressure: a sensor that stopped talking is the more urgent
    // fact, and it is the one every other field reports as healthy.
    status.summary(
      DiagnosticStatus::STALE,
      "connected but silent for " + std::to_string(*stats.last_receive_age_ms) + " ms");
  } else if (stats.backpressure_active) {
    status.summary(DiagnosticStatus::WARN, "connected, backpressure active");
  } else {
    status.summary(DiagnosticStatus::OK, "connected");
  }

  status.add("connected", connected);
  status.add("backpressure_active", stats.backpressure_active);

  status.add("bytes_accepted", stats.bytes_accepted);
  status.add("messages_accepted", stats.messages_accepted);
  status.add("bytes_sent", stats.bytes_sent);
  status.add("messages_sent", stats.messages_sent);
  status.add("bytes_received", stats.bytes_received);
  status.add("messages_received", stats.messages_received);

  status.add("failed_sends", stats.failed_sends);
  status.add("dropped_messages", stats.dropped_messages);
  status.add("dropped_bytes", stats.dropped_bytes);
  status.add("backpressure_events", stats.backpressure_events);

  if (stats.last_receive_age_ms) {
    status.add("last_receive_age_ms", *stats.last_receive_age_ms);
  } else {
    status.add("last_receive_age_ms", "never");
  }

  status.add("queued_bytes", stats.queued_bytes);
  status.add("pending_bytes", stats.pending_bytes);
  status.add("max_queued_bytes", stats.max_queued_bytes);
}

}  // namespace wirestead_ros
