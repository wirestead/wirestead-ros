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

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>

#include "wirestead_ros/visibility_control.hpp"

namespace wirestead_ros
{

/// Blocks new transport callbacks and waits for callbacks already in flight.
class WIRESTEAD_ROS_PUBLIC CallbackGate {
public:
  class WIRESTEAD_ROS_PUBLIC Lease {
public:
    Lease(const Lease &) = delete;
    Lease & operator=(const Lease &) = delete;
    Lease(Lease && other) noexcept;
    Lease & operator=(Lease && other) noexcept;
    ~Lease();

private:
    friend class CallbackGate;
    explicit Lease(CallbackGate * gate) noexcept;
    void release() noexcept;

    CallbackGate * gate_;
  };

  CallbackGate() = default;
  CallbackGate(const CallbackGate &) = delete;
  CallbackGate & operator=(const CallbackGate &) = delete;
  CallbackGate(CallbackGate &&) = delete;
  CallbackGate & operator=(CallbackGate &&) = delete;
  ~CallbackGate();

  /// Allow subsequent try_enter() calls to acquire a lease.
  void open();

  /// Reject subsequent try_enter() calls without waiting for existing leases.
  void close();

  /// Reject new leases and wait until all existing leases have been released.
  /// Must not be called by a callback holding a lease from this gate.
  void close_and_wait();

  /// Acquire callback admission while the gate is open.
  [[nodiscard]] std::optional<Lease> try_enter();

  [[nodiscard]] bool accepting() const;
  [[nodiscard]] std::size_t in_flight() const;

private:
  void leave() noexcept;

  mutable std::mutex mutex_;
  std::condition_variable drained_;
  bool accepting_{false};
  std::size_t in_flight_{0};
};

}  // namespace wirestead_ros
