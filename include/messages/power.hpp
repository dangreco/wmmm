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

/// @file power.hpp
/// @brief Typed WMMM message carrying the thermostat's power state.
///
/// The Power message (id @c 0x21) carries the on/off state in the first byte of
/// a 4-byte payload (@c 0x01 on, @c 0x00 off); the remaining three bytes are
/// zero:
///
/// @code
// clang-format off
/// +--------+--------------------------+
/// | state  | reserved                 |
/// | 1 byte |        3 bytes           |
/// +--------+--------------------------+
// clang-format on
/// @endcode

#include "message.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace unilux::message {

/// @brief Thermostat power state (WMMM message id @c 0x21).
///
/// Decode an incoming frame with @ref decode and serialise an outgoing one with
/// @ref encode; the two are exact inverses.
class Power : public unilux::Message {

public:
  /// WMMM message id for this message.
  static constexpr uint8_t ID = 0x21;
  /// Expected payload length in bytes (one state byte plus three reserved).
  static constexpr size_t PAYLOAD_SIZE = 4;

  /// Whether the thermostat is powered on.
  bool on{false};

  Power() = default;
  explicit Power(bool on) : on(on) {}

  uint8_t id() const override { return ID; }

  Frame encode() const override {
    Frame frame;
    frame.msg_id = ID;
    frame.reserved[0] = 0;
    frame.reserved[1] = 0;
    frame.reserved[2] = 0;
    frame.payload.assign(PAYLOAD_SIZE, 0);
    frame.payload[0] = on ? 0x01 : 0x00;
    return frame;
  }

  std::string to_string() const override {
    return on ? "Power(on)" : "Power(off)";
  }

  /// @brief Decode a Power message from a generic WMMM frame.
  /// @param frame The frame to interpret.
  /// @return The decoded message, or @c std::nullopt if the payload is not
  ///         exactly @ref PAYLOAD_SIZE bytes. Any non-zero state byte is
  ///         treated as on.
  static std::optional<Power> decode(const Frame &frame) {
    if (frame.payload.size() != PAYLOAD_SIZE) {
      return std::nullopt;
    }

    Power msg;
    msg.on = frame.payload[0] != 0x00;
    return msg;
  }
};

} // namespace unilux::message
