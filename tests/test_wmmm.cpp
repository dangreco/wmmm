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

#include "wmmm.hpp"

using namespace unilux;

namespace {

// Build a well-formed WMMM byte sequence: 4-byte header + payload.
std::vector<uint8_t> makeMessage(uint8_t msg_id, uint8_t r0, uint8_t r1,
                                 uint8_t r2,
                                 const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> bytes = {msg_id, r0, r1, r2};
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  return bytes;
}

// Build a WMMM frame from header fields + payload.
Frame makeFrame(uint8_t msg_id, uint8_t r0, uint8_t r1, uint8_t r2,
                const std::vector<uint8_t> &payload) {
  Frame frame{};
  frame.msg_id = msg_id;
  frame.reserved[0] = r0;
  frame.reserved[1] = r1;
  frame.reserved[2] = r2;
  frame.payload = payload;
  return frame;
}

} // namespace

// --- Decoder: happy path ---------------------------------------------------

TEST(WmmmDecoder, ZeroLengthPayload) {
  Decoder decoder;
  std::optional<Frame> frame =
      decoder.decode(makeMessage(0x11, 0x22, 0x33, 0x44, {}));
  ASSERT_TRUE(frame.has_value());
  EXPECT_EQ(frame->msg_id, 0x11);
  EXPECT_EQ(frame->reserved[0], 0x22);
  EXPECT_EQ(frame->reserved[1], 0x33);
  EXPECT_EQ(frame->reserved[2], 0x44);
  EXPECT_TRUE(frame->payload.empty());
}

TEST(WmmmDecoder, SingleBytePayload) {
  Decoder decoder;
  std::optional<Frame> frame =
      decoder.decode(makeMessage(0x01, 0x02, 0x03, 0x04, {0xAB}));
  ASSERT_TRUE(frame.has_value());
  ASSERT_EQ(frame->payload.size(), 1u);
  EXPECT_EQ(frame->payload[0], 0xAB);
}

TEST(WmmmDecoder, MultiBytePayload) {
  Decoder decoder;
  std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF};
  std::optional<Frame> frame =
      decoder.decode(makeMessage(0x10, 0x20, 0x30, 0x40, payload));
  ASSERT_TRUE(frame.has_value());
  EXPECT_EQ(frame->payload, payload);
}

TEST(WmmmDecoder, HeaderFieldsParsedInOrder) {
  Decoder decoder;
  std::optional<Frame> frame =
      decoder.decode(makeMessage(0xAA, 0xBB, 0xCC, 0xDD, {}));
  ASSERT_TRUE(frame.has_value());
  EXPECT_EQ(frame->msg_id, 0xAA);
  EXPECT_EQ(frame->reserved[0], 0xBB);
  EXPECT_EQ(frame->reserved[1], 0xCC);
  EXPECT_EQ(frame->reserved[2], 0xDD);
}

TEST(WmmmDecoder, ExactlyFourBytesIsValidFrame) {
  // A buffer of exactly the header size is a valid frame with empty payload.
  Decoder decoder;
  std::optional<Frame> frame = decoder.decode({0x01, 0x02, 0x03, 0x04});
  ASSERT_TRUE(frame.has_value());
  EXPECT_EQ(frame->msg_id, 0x01);
  EXPECT_TRUE(frame->payload.empty());
}

// --- Decoder: undersized buffers -------------------------------------------

TEST(WmmmDecoder, EmptyBufferReturnsNullopt) {
  Decoder decoder;
  EXPECT_FALSE(decoder.decode({}).has_value());
}

TEST(WmmmDecoder, ShortBufferReturnsNullopt) {
  // Buffers shorter than the 4-byte header cannot carry a frame.
  Decoder decoder;
  EXPECT_FALSE(decoder.decode({0x01}).has_value());
  EXPECT_FALSE(decoder.decode({0x01, 0x02}).has_value());
  EXPECT_FALSE(decoder.decode({0x01, 0x02, 0x03}).has_value());
}

// --- Encoder: wire layout --------------------------------------------------

TEST(WmmmEncoder, ZeroLengthPayload) {
  Encoder encoder;
  EXPECT_EQ(encoder.encode(makeFrame(0x11, 0x22, 0x33, 0x44, {})),
            (std::vector<uint8_t>{0x11, 0x22, 0x33, 0x44}));
}

TEST(WmmmEncoder, SingleBytePayload) {
  Encoder encoder;
  EXPECT_EQ(encoder.encode(makeFrame(0x01, 0x02, 0x03, 0x04, {0xAB})),
            (std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0xAB}));
}

TEST(WmmmEncoder, MultiBytePayload) {
  Encoder encoder;
  std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
  std::vector<uint8_t> bytes =
      encoder.encode(makeFrame(0x10, 0x20, 0x30, 0x40, payload));
  std::vector<uint8_t> expected = {0x10, 0x20, 0x30, 0x40};
  expected.insert(expected.end(), payload.begin(), payload.end());
  EXPECT_EQ(bytes, expected);
  EXPECT_EQ(bytes.size(), 4u + payload.size());
}

TEST(WmmmEncoder, HeaderFieldsPlacedInOrder) {
  Encoder encoder;
  std::vector<uint8_t> bytes =
      encoder.encode(makeFrame(0xAA, 0xBB, 0xCC, 0xDD, {}));
  ASSERT_EQ(bytes.size(), 4u);
  EXPECT_EQ(bytes[0], 0xAA); // msg_id
  EXPECT_EQ(bytes[1], 0xBB); // reserved[0]
  EXPECT_EQ(bytes[2], 0xCC); // reserved[1]
  EXPECT_EQ(bytes[3], 0xDD); // reserved[2]
}

// --- Encoder / Decoder round-trip ------------------------------------------

TEST(WmmmRoundTrip, ReEncodeMatchesOriginalBytes) {
  // Decoding a buffer then re-encoding the result must yield the original
  // bytes, proving the two are exact inverses.
  std::vector<uint8_t> original =
      makeMessage(0x55, 0x66, 0x77, 0x88, {0x12, 0x34, 0x56});
  Decoder decoder;
  Encoder encoder;
  std::optional<Frame> frame = decoder.decode(original);
  ASSERT_TRUE(frame.has_value());
  EXPECT_EQ(encoder.encode(*frame), original);
}

TEST(WmmmRoundTrip, ReDecodeMatchesOriginalFrame) {
  // Encoding a frame then decoding the result must reproduce the original
  // fields, including a payload whose bytes look like a header.
  std::vector<uint8_t> payload = {0x00, 0xFF, 0x01, 0x02, 0x03, 0x04};
  Frame original = makeFrame(0x7A, 0x00, 0x01, 0x02, payload);
  Encoder encoder;
  Decoder decoder;
  std::optional<Frame> roundTripped = decoder.decode(encoder.encode(original));
  ASSERT_TRUE(roundTripped.has_value());
  EXPECT_EQ(roundTripped->msg_id, original.msg_id);
  EXPECT_EQ(roundTripped->reserved[0], original.reserved[0]);
  EXPECT_EQ(roundTripped->reserved[1], original.reserved[1]);
  EXPECT_EQ(roundTripped->reserved[2], original.reserved[2]);
  EXPECT_EQ(roundTripped->payload, original.payload);
}
