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

#pragma once

/// @file aup.hpp
/// @brief Decoder and encoder for the AUP (Async UART Protocol) wire format.
///
/// AUP frames arrive byte-by-byte over an asynchronous UART link, so this
/// header provides an incremental, allocation-light state machine (@ref
/// wmmm::aup::Decoder) that reassembles one @ref wmmm::aup::Frame frame at a
/// time. The reverse direction is handled by @ref wmmm::aup::Encoder, which
/// serialises a @ref wmmm::aup::Frame back into this byte layout. The on-wire
/// frame layout is:
///
/// @code
// clang-format off
/// +--------+--------+----------+------+------+---------+----------------+---------+
/// | MAGIC1 | MAGIC2 | checksum | flag | type | command | length (BE 16) | payload |
/// |  0x5A  |  0x9E  |  1 byte  |  1B  |  1B  |   1B    |    2 bytes     | N bytes |
/// +--------+--------+----------+------+------+---------+----------------+---------+
// clang-format on
/// @endcode
///
/// The 16-bit @c length field is big-endian (most-significant byte first) and
/// gives the number of payload bytes that follow the header.
///
/// Frames decoded by @ref Decoder can be re-serialised byte-for-byte by
/// @ref Encoder, so the two are exact inverses for any well-formed frame.

#include <cstdint>
#include <optional>
#include <vector>

namespace wmmm::aup {

/// First byte of the two-byte frame start marker.
constexpr uint8_t AUP_MAGIC1 = 0x5A;
/// Second byte of the two-byte frame start marker.
constexpr uint8_t AUP_MAGIC2 = 0x9E;

/// @brief A fully decoded AUP frame.
///
/// Populated by @ref Decoder::consume once an entire frame has been received.
/// All multi-byte fields are presented in host byte order.
struct Frame {
  uint8_t checksum; ///< Frame checksum byte, as received (not verified).
  uint8_t flag;     ///< Protocol flag byte.
  uint8_t type;     ///< Message type byte.
  uint8_t command;  ///< Command byte.
  uint16_t length;  ///< Declared payload length; equals @c payload.size().
  std::vector<uint8_t>
      payload; ///< Payload bytes (empty for a zero-length frame).
};

/// @brief Incremental, byte-at-a-time parser for AUP frames.
///
/// Feed received bytes one at a time to @ref consume. The parser tracks partial
/// frame state internally and yields a completed @ref Frame only once the full
/// frame (header + declared payload) has been consumed, then resets itself for
/// the next frame.
///
/// Resynchronisation: while scanning for the start marker, any byte that does
/// not advance the @c MAGIC1 / @c MAGIC2 sequence discards the in-progress
/// match and restarts the search, so stray bytes between frames are tolerated.
/// A run of repeated @ref AUP_MAGIC1 bytes is collapsed, allowing the marker to
/// be recognised even when preceded by extra @c 0x5A bytes. Note that once the
/// header is accepted there is no in-frame resync: the next @c length bytes are
/// always treated as payload, even if they contain the magic markers.
///
/// The parser performs no checksum validation; @ref Frame::checksum is surfaced
/// verbatim for the caller to verify if desired.
///
/// @note Not thread-safe. Use one instance per UART stream.
class Decoder {
public:
  /// @brief Consume a single received byte.
  /// @param byte The next byte from the UART stream.
  /// @return The decoded frame if @p byte completed one; otherwise
  ///         @c std::nullopt while more bytes are still needed.
  std::optional<Frame> consume(uint8_t byte);

private:
  /// Position within the frame currently being assembled.
  enum class State : uint8_t {
    MAGIC1,   ///< Awaiting the first start-marker byte (@ref AUP_MAGIC1).
    MAGIC2,   ///< Awaiting the second start-marker byte (@ref AUP_MAGIC2).
    CHECKSUM, ///< Awaiting the checksum byte.
    FLAG,     ///< Awaiting the flag byte.
    TYPE,     ///< Awaiting the type byte.
    COMMAND,  ///< Awaiting the command byte.
    LENGTH1,  ///< Awaiting the length most-significant byte.
    LENGTH2,  ///< Awaiting the length least-significant byte.
    PAYLOAD,  ///< Accumulating payload bytes until @c length are read.
  };

  State state_ = State::MAGIC1; ///< Current parser position.
  Frame frame_;                 ///< Frame being assembled.
  uint16_t read_ = 0;           ///< Payload bytes accumulated so far.

  /// Restore the parser to its initial state, ready for the next frame.
  void reset() {
    state_ = State::MAGIC1;
    frame_ = {};
    read_ = 0;
  }
};

/// @brief Serialise a @ref Frame back into its on-wire byte sequence.
///
/// Produces the exact layout described at the top of this header (start marker,
/// header fields, big-endian length, payload), suitable for transmission over
/// the UART link. @ref Encoder is the inverse of @ref Decoder: a frame produced
/// by @ref Decoder::consume, when passed through @ref encode, yields the same
/// byte sequence, and vice versa.
///
/// The encoder performs no checksum computation; @ref Frame::checksum is
/// emitted verbatim, so a frame round-trips losslessly. The length field is
/// written from @ref Frame::length and the payload bytes from @ref
/// Frame::payload; for a well-formed frame these agree, and the caller is
/// responsible for keeping them consistent.
///
/// @note Not thread-safe.
class Encoder {
public:
  /// @brief Serialise a frame to its on-wire representation.
  /// @param frame The frame to encode.
  /// @return The complete byte sequence for the frame (header + payload). The
  ///         returned vector is exactly @c 8 + frame.payload.size() bytes long.
  std::vector<uint8_t> encode(const Frame &frame);
};

} // namespace wmmm::aup
