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

#include "messages/power.hpp"
#include "wmmm.hpp"

using unilux::Frame;
using unilux::Message;
using unilux::message::Power;

namespace {

// Build a WMMM frame for the Power message id with the given payload.
Frame makeFrame(const std::vector<uint8_t> &payload) {
  Frame frame{};
  frame.msg_id = Power::ID;
  frame.payload = payload;
  return frame;
}

} // namespace

// --- Identity & polymorphism -----------------------------------------------

TEST(Power, IdMatchesConstant) {
  EXPECT_EQ(Power{}.id(), Power::ID);
  EXPECT_EQ(Power::ID, 0x21);
}

TEST(Power, UsableThroughMessageBase) {
  Power power(true);
  Message &msg = power;
  EXPECT_EQ(msg.id(), Power::ID);
  EXPECT_EQ(msg.encode().msg_id, Power::ID);
}

// --- decode ----------------------------------------------------------------

TEST(PowerDecode, ParsesOnAndOff) {
  EXPECT_TRUE(Power::decode(makeFrame({0x01, 0x00, 0x00, 0x00}))->on);
  EXPECT_FALSE(Power::decode(makeFrame({0x00, 0x00, 0x00, 0x00}))->on);
}

TEST(PowerDecode, TreatsAnyNonZeroAsOn) {
  EXPECT_TRUE(Power::decode(makeFrame({0xFF, 0x00, 0x00, 0x00}))->on);
}

TEST(PowerDecode, RejectsWrongPayloadSize) {
  EXPECT_FALSE(Power::decode(makeFrame({})).has_value());
  EXPECT_FALSE(Power::decode(makeFrame({0x01})).has_value());
  EXPECT_FALSE(Power::decode(makeFrame({0x01, 0x00, 0x00})).has_value());
  EXPECT_FALSE(
      Power::decode(makeFrame({0x01, 0x00, 0x00, 0x00, 0x00})).has_value());
}

// --- encode ----------------------------------------------------------------

TEST(PowerEncode, ProducesIdAndZeroedReserved) {
  Frame frame = Power(true).encode();
  EXPECT_EQ(frame.msg_id, Power::ID);
  EXPECT_EQ(frame.reserved[0], 0);
  EXPECT_EQ(frame.reserved[1], 0);
  EXPECT_EQ(frame.reserved[2], 0);
}

TEST(PowerEncode, WritesStateByteThenZeroes) {
  EXPECT_EQ(Power(true).encode().payload,
            (std::vector<uint8_t>{0x01, 0x00, 0x00, 0x00}));
  EXPECT_EQ(Power(false).encode().payload,
            (std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00}));
}

// --- round trip ------------------------------------------------------------

TEST(PowerRoundTrip, EncodeThenDecodePreservesValue) {
  for (bool on : {true, false}) {
    std::optional<Power> decoded = Power::decode(Power(on).encode());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->on, on);
  }
}

// --- to_string -------------------------------------------------------------

TEST(PowerToString, FormatsState) {
  EXPECT_EQ(Power(true).to_string(), "Power(on)");
  EXPECT_EQ(Power(false).to_string(), "Power(off)");
}
