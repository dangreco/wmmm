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

/// @file wmmm.cpp
/// @brief Implementation of the WMMM message decoder and encoder.

#include "wmmm.hpp"
#include <optional>

namespace unilux {

std::optional<Frame> Decoder::decode(const std::vector<uint8_t> &data) {
  if (data.size() < 4) {
    return std::nullopt; // Not enough data for header
  }
  Frame frame;
  frame.msg_id = data[0];
  frame.reserved[0] = data[1];
  frame.reserved[1] = data[2];
  frame.reserved[2] = data[3];
  frame.payload.assign(data.begin() + 4, data.end());
  return frame;
};

std::vector<uint8_t> Encoder::encode(const Frame &frame) {
  std::vector<uint8_t> data;
  data.push_back(frame.msg_id);
  data.push_back(frame.reserved[0]);
  data.push_back(frame.reserved[1]);
  data.push_back(frame.reserved[2]);
  data.insert(data.end(), frame.payload.begin(), frame.payload.end());
  return data;
};

} // namespace unilux
