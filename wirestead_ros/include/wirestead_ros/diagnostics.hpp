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

#pragma once

#include <diagnostic_updater/diagnostic_updater.hpp>

#include "wirestead/wrapper/runtime_stats.hpp"
#include "wirestead_ros/visibility_control.hpp"

namespace wirestead_ros
{

/// Fills a diagnostic status from a Wirestead channel's runtime counters.
///
/// Every RuntimeStats field is added as a key/value under its own name, so
/// there is no translation table to keep in step with the struct.
///
/// The level comes only from state that is true *now*:
///
/// - not connected             -> ERROR
/// - connected, backpressured  -> WARN
/// - connected                 -> OK
///
/// `dropped_messages`, `dropped_bytes`, `failed_sends` and `backpressure_events`
/// deliberately do not raise the level. They are cumulative since the channel
/// started, so a single drop an hour ago would latch a warning for the rest of
/// the run and a caller could never clear it. To report "dropping right now",
/// diff the counter against the previous sample and set the level yourself
/// after calling this - the status is yours to amend.
///
/// Call it from a diagnostic_updater task:
///
/// ```cpp
/// updater.add("link", [this](diagnostic_updater::DiagnosticStatusWrapper & s) {
///   wirestead_ros::report_channel_stats(s, channel_->stats(), channel_->connected());
/// });
/// ```
WIRESTEAD_ROS_PUBLIC
void report_channel_stats(
  diagnostic_updater::DiagnosticStatusWrapper & status,
  const wirestead::wrapper::RuntimeStats & stats,
  bool connected);

}  // namespace wirestead_ros
