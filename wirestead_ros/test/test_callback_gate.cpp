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
#include <future>
#include <thread>

#include "wirestead_ros/callback_gate.hpp"

using namespace std::chrono_literals;

TEST(CallbackGate, StartsClosed) {
  wirestead_ros::CallbackGate gate;

  EXPECT_FALSE(gate.accepting());
  EXPECT_FALSE(gate.try_enter().has_value());
  EXPECT_EQ(gate.in_flight(), 0U);
}

TEST(CallbackGate, LeaseTracksInFlightCallback) {
  wirestead_ros::CallbackGate gate;
  gate.open();

  {
    auto lease = gate.try_enter();
    ASSERT_TRUE(lease.has_value());
    EXPECT_EQ(gate.in_flight(), 1U);
  }

  EXPECT_EQ(gate.in_flight(), 0U);
}

TEST(CallbackGate, CloseAndWaitDrainsExistingLease) {
  wirestead_ros::CallbackGate gate;
  gate.open();

  std::promise<void> entered;
  std::promise<void> release;
  auto release_future = release.get_future();
  std::thread callback([&] {
      auto lease = gate.try_enter();
      ASSERT_TRUE(lease.has_value());
      entered.set_value();
      release_future.wait();
    });

  entered.get_future().wait();
  gate.close();
  auto closing = std::async(std::launch::async, [&] {gate.close_and_wait();});

  EXPECT_EQ(closing.wait_for(50ms), std::future_status::timeout);
  EXPECT_FALSE(gate.try_enter().has_value());

  release.set_value();
  callback.join();
  EXPECT_EQ(closing.wait_for(1s), std::future_status::ready);
  EXPECT_EQ(gate.in_flight(), 0U);
}
