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

/// @file aup.cpp
/// @brief Implementation of the AUP frame decoder and encoder.

#include "aup.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace wmmm::aup {

// The parser is a linear state machine: each received byte advances `state_`
// through the frame layout (start marker -> header -> length -> payload).
// Header and payload bytes are written straight into `frame_`; the assembled
// frame is returned (and the parser reset) on the byte that completes the
// payload.
std::optional<Frame> Decoder::consume(uint8_t byte) {
  switch (state_) {
  case State::MAGIC1:
    // Hunt for the start of a frame. Any non-marker byte is stream noise that
    // we discard by staying in (resetting to) this state.
    if (byte == AUP_MAGIC1) {
      state_ = State::MAGIC2;
    } else {
      reset();
    }
    break;
  case State::MAGIC2:
    if (byte == AUP_MAGIC2) {
      // Full start marker seen; begin reading the header.
      state_ = State::CHECKSUM;
    } else if (byte == AUP_MAGIC1) {
      // Another MAGIC1: collapse runs of 0x5A so the marker is still recognised
      // when preceded by extra 0x5A bytes (e.g. 5A 5A 9E).
      state_ = State::MAGIC2;
    } else {
      // Not a valid marker continuation; resynchronise from scratch.
      reset();
    }
    break;
  case State::CHECKSUM:
    frame_.checksum = byte;
    state_ = State::FLAG;
    break;
  case State::FLAG:
    frame_.flag = byte;
    state_ = State::TYPE;
    break;
  case State::TYPE:
    frame_.type = byte;
    state_ = State::COMMAND;
    break;
  case State::COMMAND:
    frame_.command = byte;
    state_ = State::LENGTH1;
    break;
  case State::LENGTH1:
    // Big-endian: first length byte is the most significant byte (MSB).
    frame_.length = static_cast<uint16_t>(byte) << 8;
    state_ = State::LENGTH2;
    break;
  case State::LENGTH2:
    // Big-endian: second length byte is the least significant byte (LSB).
    frame_.length |= byte;
    if (frame_.length > 0) {
      // Reserve up front to avoid reallocations while streaming the payload.
      frame_.payload.reserve(frame_.length);
      state_ = State::PAYLOAD;
      break;
    } else {
      // Zero-length payload: the frame is already complete at the header.
      Frame frame = frame_;
      reset();
      return frame;
    }
  case State::PAYLOAD:
    // Accumulate payload bytes verbatim (no in-frame resync) until we have read
    // the declared length.
    frame_.payload.push_back(byte);
    read_++;
    if (read_ >= frame_.length) {
      Frame frame = frame_;
      reset();
      return frame;
    }
    break;
  }

  // No frame completed on this byte; more input is needed.
  return std::nullopt;
}

std::vector<uint8_t> Encoder::encode(const Frame &frame) {
  std::vector<uint8_t> bytes;
  bytes.reserve(8 + frame.length);

  // Magic bytes
  bytes.push_back(AUP_MAGIC1);
  bytes.push_back(AUP_MAGIC2);

  // Header fields
  bytes.push_back(frame.checksum);
  bytes.push_back(frame.flag);
  bytes.push_back(frame.type);
  bytes.push_back(frame.command);

  // Length -- convert to big-endian byte order
  bytes.push_back(static_cast<uint8_t>(frame.length >> 8));   // MSB
  bytes.push_back(static_cast<uint8_t>(frame.length & 0xFF)); // LSB

  // Payload
  bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());

  return bytes;
}

} // namespace wmmm::aup
