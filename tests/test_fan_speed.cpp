/*
 * unilux-uart -- a clean-room C++ library for the AUP/WMMM UART protocol.
 *
 * Copyright (C) 2026  Dan Greco <git@dangre.co>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "messages/fan_speed.hpp"
#include "wmmm.hpp"

using unilux::Frame;
using unilux::Message;
using unilux::message::FanSpeed;

namespace {

// Build a WMMM frame for the FanSpeed message id with the given payload.
Frame makeFrame(const std::vector<uint8_t> &payload) {
  Frame frame{};
  frame.msg_id = FanSpeed::ID;
  frame.payload = payload;
  return frame;
}

} // namespace

// --- Identity & polymorphism -----------------------------------------------

TEST(FanSpeed, IdMatchesConstant) {
  EXPECT_EQ(FanSpeed{}.id(), FanSpeed::ID);
  EXPECT_EQ(FanSpeed::ID, 0x22);
}

TEST(FanSpeed, UsableThroughMessageBase) {
  FanSpeed fan(FanSpeed::Value::High);
  Message &msg = fan;
  EXPECT_EQ(msg.id(), FanSpeed::ID);
  EXPECT_EQ(msg.encode().msg_id, FanSpeed::ID);
}

// --- decode ----------------------------------------------------------------

TEST(FanSpeedDecode, ParsesKnownSpeeds) {
  EXPECT_EQ(FanSpeed::decode(makeFrame({0x00, 0x00, 0x00, 0x00}))->value,
            FanSpeed::Value::Auto);
  EXPECT_EQ(FanSpeed::decode(makeFrame({0x01, 0x00, 0x00, 0x00}))->value,
            FanSpeed::Value::Low);
  EXPECT_EQ(FanSpeed::decode(makeFrame({0x02, 0x00, 0x00, 0x00}))->value,
            FanSpeed::Value::Medium);
  EXPECT_EQ(FanSpeed::decode(makeFrame({0x03, 0x00, 0x00, 0x00}))->value,
            FanSpeed::Value::High);
  EXPECT_EQ(FanSpeed::decode(makeFrame({0x04, 0x00, 0x00, 0x00}))->value,
            FanSpeed::Value::Off);
}

TEST(FanSpeedDecode, CarriesUnknownValueVerbatim) {
  std::optional<FanSpeed> msg =
      FanSpeed::decode(makeFrame({0x09, 0x00, 0x00, 0x00}));
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(static_cast<uint8_t>(msg->value), 0x09);
}

TEST(FanSpeedDecode, RejectsWrongPayloadSize) {
  EXPECT_FALSE(FanSpeed::decode(makeFrame({})).has_value());
  EXPECT_FALSE(FanSpeed::decode(makeFrame({0x00})).has_value());
  EXPECT_FALSE(FanSpeed::decode(makeFrame({0x00, 0x00, 0x00})).has_value());
  EXPECT_FALSE(
      FanSpeed::decode(makeFrame({0x00, 0x00, 0x00, 0x00, 0x00})).has_value());
}

// --- encode ----------------------------------------------------------------

TEST(FanSpeedEncode, ProducesIdAndZeroedReserved) {
  Frame frame = FanSpeed(FanSpeed::Value::Medium).encode();
  EXPECT_EQ(frame.msg_id, FanSpeed::ID);
  EXPECT_EQ(frame.reserved[0], 0);
  EXPECT_EQ(frame.reserved[1], 0);
  EXPECT_EQ(frame.reserved[2], 0);
}

TEST(FanSpeedEncode, WritesSpeedByteThenZeroes) {
  EXPECT_EQ(FanSpeed(FanSpeed::Value::Auto).encode().payload,
            (std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00}));
  EXPECT_EQ(FanSpeed(FanSpeed::Value::High).encode().payload,
            (std::vector<uint8_t>{0x03, 0x00, 0x00, 0x00}));
  EXPECT_EQ(FanSpeed(FanSpeed::Value::Off).encode().payload,
            (std::vector<uint8_t>{0x04, 0x00, 0x00, 0x00}));
}

// --- round trip ------------------------------------------------------------

TEST(FanSpeedRoundTrip, EncodeThenDecodePreservesValue) {
  for (FanSpeed::Value v :
       {FanSpeed::Value::Auto, FanSpeed::Value::Low, FanSpeed::Value::Medium,
        FanSpeed::Value::High, FanSpeed::Value::Off}) {
    std::optional<FanSpeed> decoded = FanSpeed::decode(FanSpeed(v).encode());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->value, v);
  }
}

// --- to_string -------------------------------------------------------------

TEST(FanSpeedToString, FormatsKnownSpeeds) {
  EXPECT_EQ(FanSpeed(FanSpeed::Value::Auto).to_string(), "FanSpeed(auto)");
  EXPECT_EQ(FanSpeed(FanSpeed::Value::Low).to_string(), "FanSpeed(low)");
  EXPECT_EQ(FanSpeed(FanSpeed::Value::Medium).to_string(), "FanSpeed(medium)");
  EXPECT_EQ(FanSpeed(FanSpeed::Value::High).to_string(), "FanSpeed(high)");
  EXPECT_EQ(FanSpeed(FanSpeed::Value::Off).to_string(), "FanSpeed(off)");
}

TEST(FanSpeedToString, FormatsUnknownAsHex) {
  FanSpeed fan;
  fan.value = static_cast<FanSpeed::Value>(0x09);
  EXPECT_EQ(fan.to_string(), "FanSpeed(0x09)");
}
