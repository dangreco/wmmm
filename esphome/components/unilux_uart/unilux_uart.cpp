#include "unilux_uart.h"

namespace esphome {
namespace unilux_uart {

static const char *const TAG = "unilux_uart";

void UniluxUartComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Unilux UART:");
  check_uart_settings(115200);
}

void UniluxUartComponent::setup() {
  // Plumbing check: forces src/aup.cpp to link. Real decode wiring is a
  // follow-up.
  (void)decoder_.consume(0x00);
}

void UniluxUartComponent::loop() {}

} // namespace unilux_uart
} // namespace esphome
