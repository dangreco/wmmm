#include "unilux_uart.h"

#include "esphome/core/alloc_helpers.h"
#include "esphome/core/log.h"

#include "messages.hpp"

#include <type_traits>
#include <variant>
#include <vector>

namespace esphome {
namespace unilux_uart {

static const char *const TAG = "unilux_uart";

void UniluxUartComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Unilux UART:");
  check_uart_settings(115200);
}

void UniluxUartComponent::setup() {}

void UniluxUartComponent::loop() {
  uint8_t byte;
  while (this->available() && this->read_byte(&byte)) {
    if (auto frame = decoder_.consume(byte)) {
      log_frame_(*frame);
    }
  }
}

void UniluxUartComponent::log_frame_(const unilux::aup::Frame &frame) {
  ESP_LOGD(TAG, "┌─ frame ───────────────────────────");
  ESP_LOGD(TAG,
           "│ AUP  type=0x%02X cmd=0x%02X flag=0x%02X chksum=0x%02X len=%u",
           frame.type, frame.command, frame.flag, frame.checksum, frame.length);

  if (auto wmmm = wmmm_decoder_.decode(frame.payload)) {
    std::vector<uint8_t> reserved(wmmm->reserved, wmmm->reserved + 3);
    ESP_LOGD(TAG, "│ WMMM msg_id=0x%02X  reserved=%s", wmmm->msg_id,
             format_hex_pretty(reserved, '.', false).c_str());
    ESP_LOGD(TAG, "│ payload (%u): %s", (unsigned)wmmm->payload.size(),
             format_hex_pretty(wmmm->payload, ' ', false).c_str());

    // Decode the WMMM frame into a concrete message type and switch on it.
    if (auto msg = unilux::decode_message(*wmmm)) {
      std::visit(
          [&](auto &&m) {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, unilux::message::Temperature>) {
              ESP_LOGD(TAG, "│ Temperature t1=%.1f°C t2=%.1f°C",
                       static_cast<double>(m.t1), static_cast<double>(m.t2));
              if (this->t1_sensor_ != nullptr)
                this->t1_sensor_->publish_state(m.t1);
              if (this->t2_sensor_ != nullptr)
                this->t2_sensor_->publish_state(m.t2);
            }
          },
          *msg);
    } else {
      ESP_LOGD(TAG, "│ <no typed message for id 0x%02X>", wmmm->msg_id);
    }
  } else {
    // Payload too short to be a WMMM message; show the raw AUP payload instead.
    ESP_LOGD(TAG, "│ WMMM <undecodable, %u bytes>",
             (unsigned)frame.payload.size());
    ESP_LOGD(TAG, "│ payload (%u): %s", (unsigned)frame.payload.size(),
             format_hex_pretty(frame.payload, ' ', false).c_str());
  }
  ESP_LOGD(TAG, "└───────────────────────────────────");
}

} // namespace unilux_uart
} // namespace esphome
