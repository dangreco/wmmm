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

#pragma once

/// @file wmmm.hpp
/// @brief Decoder and encoder for the WMMM message layer.
///
/// A WMMM message is the payload carried by an AUP frame (see @c aup.hpp): the
/// AUP layer (@ref unilux::aup::AupParser) supplies byte framing and length,
/// while this layer gives that payload its internal structure. Each message is
/// a 4-byte header followed by a variable-length payload:
///
/// @code
// clang-format off
/// +--------+-------------------+----------------+
/// | msg_id | reserved (3 bytes)| payload        |
/// | 1 byte |     3 bytes       | N bytes        |
/// +--------+-------------------+----------------+
// clang-format on
/// @endcode
///
/// The header's first byte is the message id; the remaining three header bytes
/// are reserved and carried verbatim (neither interpreted nor validated). This
/// header provides @ref unilux::Decoder, which reassembles a @ref unilux::Frame
/// from a complete byte buffer, and @ref unilux::Encoder, the exact inverse
/// that serialises a frame back to its on-wire representation.

#include <cstdint>
#include <optional>
#include <vector>

namespace unilux {

/// @brief A fully decoded WMMM message.
///
/// Populated by @ref Decoder::decode. The 4-byte header is split into the
/// message id and three reserved bytes; the trailing bytes become the payload.
struct Frame {
  uint8_t msg_id;      ///< Message id byte (header byte 0).
  uint8_t reserved[3]; ///< Reserved header bytes (1..3), carried verbatim.
  std::vector<uint8_t> payload; ///< Payload bytes following the 4-byte header.
};

/// @brief Reassembles a WMMM message from a complete byte buffer.
///
/// Unlike the streaming @ref unilux::aup::AupParser, @ref decode expects the
/// full message (header + payload) to already be present in @p data, since
/// framing and length are handled by the enclosing AUP layer. A buffer shorter
/// than the 4-byte header yields @c std::nullopt; otherwise the header is
/// consumed and every remaining byte is treated as payload.
///
/// @note Not thread-safe.
class Decoder {
public:
  /// @brief Decode a complete WMMM message from @p data.
  /// @param data The full message bytes (at least the 4-byte header).
  /// @return The decoded frame, or @c std::nullopt if @p data is shorter than
  ///         the 4-byte header.
  std::optional<Frame> decode(const std::vector<uint8_t> &data);
};

/// @brief Serialises a @ref Frame back into its on-wire byte sequence.
///
/// Produces the layout described at the top of this header (4-byte header
/// followed by the payload). @ref Encoder is the inverse of @ref Decoder: a
/// frame produced by @ref Decoder::decode, when passed through @ref encode,
/// yields the original byte sequence, and vice versa.
///
/// @note Not thread-safe.
class Encoder {
public:
  /// @brief Serialise a frame to its on-wire representation.
  /// @param frame The frame to encode.
  /// @return The complete byte sequence for the frame (header + payload). The
  ///         returned vector is exactly @c 4 + frame.payload.size() bytes long.
  std::vector<uint8_t> encode(const Frame &frame);
};

} // namespace unilux
