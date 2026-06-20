/*
 * wmmm -- a clean-room C++ library for the AUP/WMMM UART protocol.
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

#include "aup.hpp"

using namespace wmmm::aup;

namespace {

// Feed bytes that should NOT complete a frame; every byte must yield nullopt.
void feedNone(Decoder &parser, const std::vector<uint8_t> &bytes) {
  for (uint8_t byte : bytes) {
    EXPECT_FALSE(parser.consume(byte).has_value());
  }
}

// Feed a complete frame: every byte but the last yields nullopt, and the last
// byte yields the assembled frame. Returns the frame.
Frame feedFrame(Decoder &parser, const std::vector<uint8_t> &bytes) {
  EXPECT_FALSE(bytes.empty());
  for (size_t i = 0; i + 1 < bytes.size(); ++i) {
    EXPECT_FALSE(parser.consume(bytes[i]).has_value())
        << "frame completed early at byte index " << i;
  }
  std::optional<Frame> frame = parser.consume(bytes.back());
  EXPECT_TRUE(frame.has_value()) << "frame did not complete on final byte";
  return frame.value_or(Frame{});
}

// Build a well-formed AUP frame from header fields + payload.
std::vector<uint8_t> makeFrame(uint8_t checksum, uint8_t flag, uint8_t type,
                               uint8_t command,
                               const std::vector<uint8_t> &payload) {
  uint16_t length = static_cast<uint16_t>(payload.size());
  std::vector<uint8_t> bytes = {
      AUP_MAGIC1,
      AUP_MAGIC2,
      checksum,
      flag,
      type,
      command,
      static_cast<uint8_t>(length >> 8),
      static_cast<uint8_t>(length & 0xFF),
  };
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  return bytes;
}

} // namespace

// --- Happy path: payload sizes ---------------------------------------------

TEST(Frame, ZeroLengthPayload) {
  Decoder parser;
  Frame frame = feedFrame(
      parser, {AUP_MAGIC1, AUP_MAGIC2, 0x11, 0x22, 0x33, 0x44, 0x00, 0x00});
  EXPECT_EQ(frame.checksum, 0x11);
  EXPECT_EQ(frame.flag, 0x22);
  EXPECT_EQ(frame.type, 0x33);
  EXPECT_EQ(frame.command, 0x44);
  EXPECT_EQ(frame.length, 0u);
  EXPECT_TRUE(frame.payload.empty());
}

TEST(Frame, SingleBytePayload) {
  Decoder parser;
  Frame frame = feedFrame(parser, makeFrame(0x01, 0x02, 0x03, 0x04, {0xAB}));
  ASSERT_EQ(frame.payload.size(), 1u);
  EXPECT_EQ(frame.payload[0], 0xAB);
}

TEST(Frame, MultiBytePayload) {
  Decoder parser;
  std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF};
  Frame frame = feedFrame(parser, makeFrame(0x10, 0x20, 0x30, 0x40, payload));
  EXPECT_EQ(frame.payload, payload);
}

// --- Header field parsing ---------------------------------------------------

TEST(Frame, FieldsParsedCorrectly) {
  Decoder parser;
  std::vector<uint8_t> payload(0x0102, 0x7A); // 258 bytes
  Frame frame = feedFrame(parser, makeFrame(0xAA, 0xBB, 0xCC, 0xDD, payload));
  EXPECT_EQ(frame.checksum, 0xAA);
  EXPECT_EQ(frame.flag, 0xBB);
  EXPECT_EQ(frame.type, 0xCC);
  EXPECT_EQ(frame.command, 0xDD);
  EXPECT_EQ(frame.length, 0x0102u);
  EXPECT_EQ(frame.payload.size(), 0x0102u);
  EXPECT_EQ(frame.length, frame.payload.size());
}

TEST(Frame, HeaderFieldsMayEqualMagic) {
  // Bytes equal to the magic markers in header positions are consumed
  // literally, not treated as the start of a new frame.
  Decoder parser;
  Frame frame = feedFrame(parser, makeFrame(AUP_MAGIC1, AUP_MAGIC2, AUP_MAGIC1,
                                            AUP_MAGIC2, {0x01}));
  EXPECT_EQ(frame.checksum, AUP_MAGIC1);
  EXPECT_EQ(frame.flag, AUP_MAGIC2);
  EXPECT_EQ(frame.type, AUP_MAGIC1);
  EXPECT_EQ(frame.command, AUP_MAGIC2);
  ASSERT_EQ(frame.payload.size(), 1u);
  EXPECT_EQ(frame.payload[0], 0x01);
}

// --- Length field (big-endian) ---------------------------------------------

TEST(Frame, BigEndianLength) {
  // length = 0x0102 = 258; the frame must complete on exactly the 258th payload
  // byte, proving MSB << 8 | LSB ordering.
  Decoder parser;
  std::vector<uint8_t> payload(258, 0x5C);
  std::vector<uint8_t> bytes = makeFrame(0, 0, 0, 0, payload);
  EXPECT_EQ(bytes[6], 0x01); // len_hi
  EXPECT_EQ(bytes[7], 0x02); // len_lo
  Frame frame = feedFrame(parser, bytes);
  EXPECT_EQ(frame.length, 0x0102u);
  EXPECT_EQ(frame.payload.size(), 258u);
}

TEST(Frame, LengthHighByteOnly) {
  // length = 0x0100 = 256; a zero LSB must not be mistaken for a zero-length
  // (no-payload) frame.
  Decoder parser;
  std::vector<uint8_t> payload(256, 0x42);
  std::vector<uint8_t> bytes = makeFrame(0, 0, 0, 0, payload);
  EXPECT_EQ(bytes[6], 0x01); // len_hi
  EXPECT_EQ(bytes[7], 0x00); // len_lo
  Frame frame = feedFrame(parser, bytes);
  EXPECT_EQ(frame.length, 0x0100u);
  EXPECT_EQ(frame.payload.size(), 256u);
}

// --- Incremental delivery ---------------------------------------------------

TEST(Frame, NoFrameUntilComplete) {
  Decoder parser;
  std::vector<uint8_t> bytes = makeFrame(0x01, 0x02, 0x03, 0x04, {0xAA, 0xBB});
  for (size_t i = 0; i + 1 < bytes.size(); ++i) {
    EXPECT_FALSE(parser.consume(bytes[i]).has_value()) << "early at " << i;
  }
  EXPECT_TRUE(parser.consume(bytes.back()).has_value());
}

// --- Resync / garbage handling ---------------------------------------------

TEST(Frame, GarbageBeforeFrame) {
  Decoder parser;
  feedNone(parser, {0x00, 0x01, 0x99, 0xFF, 0x12}); // never see MAGIC1
  Frame frame = feedFrame(parser, makeFrame(0x55, 0x66, 0x77, 0x88, {0xCD}));
  EXPECT_EQ(frame.checksum, 0x55);
  ASSERT_EQ(frame.payload.size(), 1u);
  EXPECT_EQ(frame.payload[0], 0xCD);
}

TEST(Frame, Magic1ResetOnBadByte) {
  // MAGIC1 then a non-magic byte resets; the parser must still parse the next
  // valid frame.
  Decoder parser;
  feedNone(parser, {AUP_MAGIC1, 0x00});
  Frame frame = feedFrame(parser, makeFrame(0x01, 0x02, 0x03, 0x04, {0xEE}));
  ASSERT_EQ(frame.payload.size(), 1u);
  EXPECT_EQ(frame.payload[0], 0xEE);
}

TEST(Frame, Magic2BadByteResets) {
  // MAGIC1 then a byte that is neither MAGIC2 nor MAGIC1 resets to MAGIC1.
  Decoder parser;
  feedNone(parser, {AUP_MAGIC1, 0x12});
  Frame frame = feedFrame(parser, makeFrame(0x09, 0x08, 0x07, 0x06, {0x5A}));
  EXPECT_EQ(frame.checksum, 0x09);
  ASSERT_EQ(frame.payload.size(), 1u);
  EXPECT_EQ(frame.payload[0], 0x5A);
}

TEST(Frame, DoubleMagic1) {
  // 5A 5A 9E ... -- a second MAGIC1 keeps the parser in MAGIC2.
  Decoder parser;
  std::vector<uint8_t> bytes = {AUP_MAGIC1, AUP_MAGIC1, AUP_MAGIC2, 0x01, 0x02,
                                0x03,       0x04,       0x00,       0x00};
  Frame frame = feedFrame(parser, bytes);
  EXPECT_EQ(frame.checksum, 0x01);
  EXPECT_TRUE(frame.payload.empty());
}

TEST(Frame, ManyMagic1) {
  Decoder parser;
  std::vector<uint8_t> bytes = {AUP_MAGIC1, AUP_MAGIC1, AUP_MAGIC1, AUP_MAGIC1,
                                AUP_MAGIC2, 0xAA,       0xBB,       0xCC,
                                0xDD,       0x00,       0x01,       0x7E};
  Frame frame = feedFrame(parser, bytes);
  EXPECT_EQ(frame.checksum, 0xAA);
  ASSERT_EQ(frame.payload.size(), 1u);
  EXPECT_EQ(frame.payload[0], 0x7E);
}

TEST(Frame, PartialThenResync) {
  // A partial header is abandoned by a bad byte in MAGIC2, then a full valid
  // frame is parsed.
  Decoder parser;
  feedNone(parser, {AUP_MAGIC1, 0x01}); // bad MAGIC2 -> reset
  Frame frame =
      feedFrame(parser, makeFrame(0x11, 0x22, 0x33, 0x44, {0x99, 0x88}));
  EXPECT_EQ(frame.payload, (std::vector<uint8_t>{0x99, 0x88}));
}

// --- Magic bytes inside the body -------------------------------------------

TEST(Frame, PayloadContainsMagicBytes) {
  // Once past the header, MAGIC1/MAGIC2 bytes in the payload are pure data;
  // they must not trigger a resync.
  Decoder parser;
  std::vector<uint8_t> payload = {AUP_MAGIC1, AUP_MAGIC2, AUP_MAGIC1,
                                  AUP_MAGIC2, 0x00,       AUP_MAGIC1};
  Frame frame = feedFrame(parser, makeFrame(0x01, 0x02, 0x03, 0x04, payload));
  EXPECT_EQ(frame.payload, payload);
}

// --- Stream reuse / reset ---------------------------------------------------

TEST(Frame, BackToBackFrames) {
  Decoder parser;
  Frame first =
      feedFrame(parser, makeFrame(0x01, 0x02, 0x03, 0x04, {0x10, 0x11}));
  EXPECT_EQ(first.payload, (std::vector<uint8_t>{0x10, 0x11}));

  Frame second =
      feedFrame(parser, makeFrame(0xA1, 0xA2, 0xA3, 0xA4, {0x20, 0x21, 0x22}));
  EXPECT_EQ(second.checksum, 0xA1);
  EXPECT_EQ(second.payload, (std::vector<uint8_t>{0x20, 0x21, 0x22}));
}

TEST(Frame, ReuseLargeThenZero) {
  // A large-payload frame followed by a zero-payload frame: reset() must clear
  // the previous payload so no stale bytes leak into the second frame.
  Decoder parser;
  std::vector<uint8_t> big(64, 0xC3);
  Frame first = feedFrame(parser, makeFrame(0x01, 0x02, 0x03, 0x04, big));
  EXPECT_EQ(first.payload.size(), 64u);

  Frame second = feedFrame(parser, makeFrame(0x05, 0x06, 0x07, 0x08, {}));
  EXPECT_EQ(second.checksum, 0x05);
  EXPECT_TRUE(second.payload.empty());
}

// --- Fresh parser determinism (depends on state_ init in aup.hpp) ----------

TEST(Frame, FreshParserStartsAtMagic1) {
  Decoder parser;
  EXPECT_FALSE(
      parser.consume(0x00).has_value()); // ignored as pre-frame garbage
  Frame frame = feedFrame(parser, makeFrame(0x01, 0x02, 0x03, 0x04, {0x7F}));
  ASSERT_EQ(frame.payload.size(), 1u);
  EXPECT_EQ(frame.payload[0], 0x7F);
}

// --- Encoder: wire layout --------------------------------------------------
// The encoder must reproduce the canonical byte layout the decoder expects
// (and that makeFrame() builds), so most cases assert equality against
// makeFrame() for the same fields.

TEST(Encoder, ZeroLengthPayload) {
  Frame frame{0x11, 0x22, 0x33, 0x44, 0x0000, {}};
  Encoder encoder;
  EXPECT_EQ(encoder.encode(frame),
            (std::vector<uint8_t>{AUP_MAGIC1, AUP_MAGIC2, 0x11, 0x22, 0x33,
                                  0x44, 0x00, 0x00}));
  EXPECT_EQ(encoder.encode(frame), makeFrame(0x11, 0x22, 0x33, 0x44, {}));
}

TEST(Encoder, SingleBytePayload) {
  Frame frame{0x01, 0x02, 0x03, 0x04, 0x0001, {0xAB}};
  Encoder encoder;
  EXPECT_EQ(encoder.encode(frame), makeFrame(0x01, 0x02, 0x03, 0x04, {0xAB}));
}

TEST(Encoder, MultiBytePayload) {
  std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF};
  Frame frame{0x10,   0x20, 0x30, 0x40, static_cast<uint16_t>(payload.size()),
              payload};
  Encoder encoder;
  EXPECT_EQ(encoder.encode(frame), makeFrame(0x10, 0x20, 0x30, 0x40, payload));
}

// --- Encoder: header field placement ---------------------------------------

TEST(Encoder, HeaderFieldsPlacedInOrder) {
  Frame frame{0xAA, 0xBB, 0xCC, 0xDD, 0x0000, {}};
  Encoder encoder;
  std::vector<uint8_t> bytes = encoder.encode(frame);
  ASSERT_EQ(bytes.size(), 8u);
  EXPECT_EQ(bytes[0], AUP_MAGIC1); // magic1
  EXPECT_EQ(bytes[1], AUP_MAGIC2); // magic2
  EXPECT_EQ(bytes[2], 0xAA);       // checksum
  EXPECT_EQ(bytes[3], 0xBB);       // flag
  EXPECT_EQ(bytes[4], 0xCC);       // type
  EXPECT_EQ(bytes[5], 0xDD);       // command
  EXPECT_EQ(bytes[6], 0x00);       // length MSB
  EXPECT_EQ(bytes[7], 0x00);       // length LSB
}

TEST(Encoder, HeaderFieldsMayEqualMagic) {
  // Bytes equal to the magic markers are emitted literally as header fields.
  Frame frame{AUP_MAGIC1, AUP_MAGIC2, AUP_MAGIC1, AUP_MAGIC2, 0x0000, {}};
  Encoder encoder;
  std::vector<uint8_t> bytes = encoder.encode(frame);
  ASSERT_EQ(bytes.size(), 8u);
  EXPECT_EQ(bytes[2], AUP_MAGIC1);
  EXPECT_EQ(bytes[3], AUP_MAGIC2);
  EXPECT_EQ(bytes[4], AUP_MAGIC1);
  EXPECT_EQ(bytes[5], AUP_MAGIC2);
}

// --- Encoder: length field (big-endian) ------------------------------------

TEST(Encoder, BigEndianLength) {
  // length = 0x0102 = 258; MSB must precede LSB.
  std::vector<uint8_t> payload(0x0102, 0x5C);
  Frame frame{0, 0, 0, 0, static_cast<uint16_t>(payload.size()), payload};
  Encoder encoder;
  std::vector<uint8_t> bytes = encoder.encode(frame);
  ASSERT_EQ(bytes.size(), 8u + payload.size());
  EXPECT_EQ(bytes[6], 0x01); // len_hi
  EXPECT_EQ(bytes[7], 0x02); // len_lo
}

TEST(Encoder, LengthHighByteOnly) {
  // length = 0x0100 = 256; a zero LSB must still be emitted, not truncated.
  std::vector<uint8_t> payload(0x0100, 0x42);
  Frame frame{0, 0, 0, 0, static_cast<uint16_t>(payload.size()), payload};
  Encoder encoder;
  std::vector<uint8_t> bytes = encoder.encode(frame);
  ASSERT_GE(bytes.size(), 8u);
  EXPECT_EQ(bytes[6], 0x01); // len_hi
  EXPECT_EQ(bytes[7], 0x00); // len_lo
}

// --- Encoder: payload integrity --------------------------------------------

TEST(Encoder, PayloadMagicBytesPreservedVerbatim) {
  // Magic bytes inside the payload are pure data and must pass through
  // unchanged -- they are not escaped or stripped.
  std::vector<uint8_t> payload = {AUP_MAGIC1, AUP_MAGIC2, AUP_MAGIC1,
                                  AUP_MAGIC2, 0x00,       AUP_MAGIC1};
  Frame frame{0x01,   0x02, 0x03, 0x04, static_cast<uint16_t>(payload.size()),
              payload};
  Encoder encoder;
  std::vector<uint8_t> bytes = encoder.encode(frame);
  ASSERT_EQ(bytes.size(), 8u + payload.size());
  std::vector<uint8_t> body(bytes.begin() + 8, bytes.end());
  EXPECT_EQ(body, payload);
}

// --- Encoder / Decoder round-trip ------------------------------------------

TEST(Encoder, RoundTripsThroughDecoder) {
  // Encoding a frame and feeding the result back through the decoder must
  // yield a frame equal to the original. Exercises the magic collision case
  // (payload containing 0x5A 0x9E) to prove no resync occurs on the wire.
  std::vector<uint8_t> payload = {0x12, AUP_MAGIC1, 0x00, AUP_MAGIC2, 0xFF};
  Frame original{
      0x77, 0x88, 0x99, 0xAA, static_cast<uint16_t>(payload.size()), payload};

  Encoder encoder;
  Decoder decoder;
  std::vector<uint8_t> wire = encoder.encode(original);
  for (size_t i = 0; i + 1 < wire.size(); ++i) {
    ASSERT_FALSE(decoder.consume(wire[i]).has_value()) << "early at " << i;
  }
  std::optional<Frame> roundTripped = decoder.consume(wire.back());
  ASSERT_TRUE(roundTripped.has_value());
  EXPECT_EQ(roundTripped->checksum, original.checksum);
  EXPECT_EQ(roundTripped->flag, original.flag);
  EXPECT_EQ(roundTripped->type, original.type);
  EXPECT_EQ(roundTripped->command, original.command);
  EXPECT_EQ(roundTripped->length, original.length);
  EXPECT_EQ(roundTripped->payload, original.payload);
}

TEST(Encoder, RoundTripsZeroLengthPayload) {
  // A zero-length frame completes on the final header byte (LENGTH2), so the
  // round-trip must still deliver it -- not hang waiting for payload.
  Frame original{0x01, 0x02, 0x03, 0x04, 0x0000, {}};
  Encoder encoder;
  Decoder decoder;
  std::vector<uint8_t> wire = encoder.encode(original);
  ASSERT_EQ(wire.size(), 8u);
  for (size_t i = 0; i + 1 < wire.size(); ++i) {
    ASSERT_FALSE(decoder.consume(wire[i]).has_value());
  }
  std::optional<Frame> roundTripped = decoder.consume(wire.back());
  ASSERT_TRUE(roundTripped.has_value());
  EXPECT_EQ(roundTripped->checksum, original.checksum);
  EXPECT_EQ(roundTripped->flag, original.flag);
  EXPECT_EQ(roundTripped->type, original.type);
  EXPECT_EQ(roundTripped->command, original.command);
  EXPECT_EQ(roundTripped->length, 0u);
  EXPECT_TRUE(roundTripped->payload.empty());
}
