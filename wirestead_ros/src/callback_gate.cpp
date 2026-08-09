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

#include "wirestead_ros/callback_gate.hpp"

#include <utility>

namespace wirestead_ros
{

CallbackGate::Lease::Lease(CallbackGate * gate) noexcept
: gate_(gate) {}

CallbackGate::Lease::Lease(Lease && other) noexcept
: gate_(std::exchange(other.gate_, nullptr)) {}

CallbackGate::Lease & CallbackGate::Lease::operator=(Lease && other) noexcept
{
  if (this != &other) {
    release();
    gate_ = std::exchange(other.gate_, nullptr);
  }
  return *this;
}

CallbackGate::Lease::~Lease() {release();}

void CallbackGate::Lease::release() noexcept
{
  if (gate_ != nullptr) {
    gate_->leave();
    gate_ = nullptr;
  }
}

CallbackGate::~CallbackGate() {close_and_wait();}

void CallbackGate::open()
{
  std::lock_guard lock(mutex_);
  accepting_ = true;
}

void CallbackGate::close()
{
  std::lock_guard lock(mutex_);
  accepting_ = false;
}

void CallbackGate::close_and_wait()
{
  std::unique_lock lock(mutex_);
  accepting_ = false;
  drained_.wait(lock, [this] {return in_flight_ == 0;});
}

std::optional<CallbackGate::Lease> CallbackGate::try_enter()
{
  std::lock_guard lock(mutex_);
  if (!accepting_) {
    return std::nullopt;
  }
  ++in_flight_;
  return Lease(this);
}

bool CallbackGate::accepting() const
{
  std::lock_guard lock(mutex_);
  return accepting_;
}

std::size_t CallbackGate::in_flight() const
{
  std::lock_guard lock(mutex_);
  return in_flight_;
}

void CallbackGate::leave() noexcept
{
  std::lock_guard lock(mutex_);
  --in_flight_;
  if (in_flight_ == 0) {
    drained_.notify_all();
  }
}

}  // namespace wirestead_ros
