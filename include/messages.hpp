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

/// @file messages.hpp
/// @brief Decode a generic WMMM frame into its concrete typed message.
///
/// The @c wmmm.hpp layer produces a generic @ref unilux::Frame (a message id,
/// three reserved bytes, and an opaque payload). Each concrete message type
/// (see the @c messages/ directory) knows how to decode itself from such a
/// frame via its own static factory. This header ties them together: given any
/// frame, @ref unilux::decode_message inspects @c msg_id and returns the
/// matching typed message, so callers can switch on the message type without
/// hard-coding id checks.
///
/// The set of known messages is the single source of truth, expressed once as
/// @ref unilux::AnyMessage. Adding a new message type is a one-line change:
/// include its header below and add it to the variant; dispatch follows
/// automatically (see @c src/messages.cpp).
///
/// Typical use, switching on the decoded type with @c std::visit:
/// @code
/// if (auto msg = unilux::decode_message(frame)) {
///   std::visit([&](auto &&m) {
///     using T = std::decay_t<decltype(m)>;
///     if constexpr (std::is_same_v<T, unilux::message::Temperature>) {
///       // m.t1, m.t2 ...
///     }
///   }, *msg);
/// }
/// @endcode

#include "message.hpp"
#include "messages/fan_speed.hpp"
#include "messages/mode.hpp"
#include "messages/power.hpp"
#include "messages/target_temperature.hpp"
#include "messages/temperature.hpp"
#include "wmmm.hpp"

#include <optional>
#include <variant>

namespace unilux {

/// @brief A decoded WMMM message, as one of the known concrete message types.
///
/// This variant is the single source of truth for the set of message types the
/// library can decode; @ref decode_message dispatches over its alternatives.
using AnyMessage =
    std::variant<message::Temperature, message::TargetTemperature,
                 message::Mode, message::Power, message::FanSpeed>;

/// @brief Decode a frame into its concrete message type, selected by @c msg_id.
/// @param frame The generic WMMM frame to interpret.
/// @return The typed message wrapped in @ref AnyMessage, or @c std::nullopt for
///         an unknown message id, or a known id whose payload fails that
///         message's own @c decode (e.g. a wrong-length payload).
std::optional<AnyMessage> decode_message(const Frame &frame);

} // namespace unilux
