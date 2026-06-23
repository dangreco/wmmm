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

/// @file fan_speed.hpp
/// @brief Typed WMMM message carrying the thermostat's fan speed.
///
/// The FanSpeed message (id @c 0x22) carries the fan setting in the first byte
/// of a 4-byte payload; the remaining three bytes are zero:
///
/// @code
// clang-format off
/// +--------+--------------------------+
/// | speed  | reserved                 |
/// | 1 byte |        3 bytes           |
/// +--------+--------------------------+
// clang-format on
/// @endcode
///
/// The speed byte is one of @ref FanSpeed::Value (auto, low, medium, high,
/// off). Unknown byte values are preserved verbatim by @ref decode.

#include "message.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

namespace unilux::message {

/// @brief Thermostat fan speed (WMMM message id @c 0x22).
///
/// Decode an incoming frame with @ref decode and serialise an outgoing one with
/// @ref encode; the two are exact inverses.
class FanSpeed : public unilux::Message {

public:
  /// WMMM message id for this message.
  static constexpr uint8_t ID = 0x22;
  /// Expected payload length in bytes (one speed byte plus three reserved).
  static constexpr size_t PAYLOAD_SIZE = 4;

  /// On-wire fan speed values (the first payload byte).
  enum class Value : uint8_t {
    Auto = 0x00,   ///< Fan speed chosen automatically.
    Low = 0x01,    ///< Low fan speed.
    Medium = 0x02, ///< Medium fan speed.
    High = 0x03,   ///< High fan speed.
    Off = 0x04,    ///< Fan off.
  };

  /// The decoded fan speed value.
  Value value{Value::Auto};

  FanSpeed() = default;
  explicit FanSpeed(Value value) : value(value) {}

  uint8_t id() const override { return ID; }

  Frame encode() const override {
    Frame frame;
    frame.msg_id = ID;
    frame.reserved[0] = 0;
    frame.reserved[1] = 0;
    frame.reserved[2] = 0;
    frame.payload.assign(PAYLOAD_SIZE, 0);
    frame.payload[0] = static_cast<uint8_t>(value);
    return frame;
  }

  std::string to_string() const override {
    switch (value) {
    case Value::Auto:
      return "FanSpeed(auto)";
    case Value::Low:
      return "FanSpeed(low)";
    case Value::Medium:
      return "FanSpeed(medium)";
    case Value::High:
      return "FanSpeed(high)";
    case Value::Off:
      return "FanSpeed(off)";
    }
    char buf[28];
    std::snprintf(buf, sizeof(buf), "FanSpeed(0x%02X)",
                  static_cast<unsigned>(value));
    return buf;
  }

  /// @brief Decode a FanSpeed message from a generic WMMM frame.
  /// @param frame The frame to interpret.
  /// @return The decoded message, or @c std::nullopt if the payload is not
  ///         exactly @ref PAYLOAD_SIZE bytes. The speed byte is not validated;
  ///         an unrecognised value is carried through verbatim.
  static std::optional<FanSpeed> decode(const Frame &frame) {
    if (frame.payload.size() != PAYLOAD_SIZE) {
      return std::nullopt;
    }

    FanSpeed msg;
    msg.value = static_cast<Value>(frame.payload[0]);
    return msg;
  }
};

} // namespace unilux::message
