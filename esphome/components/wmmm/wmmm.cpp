#include "wmmm.h"

namespace esphome {
namespace wmmm {

static const char *const TAG = "wmmm";

void WmmmComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "WMMM:");
  check_uart_settings(115200);
}

void WmmmComponent::setup() {}

void WmmmComponent::loop() {}

} // namespace wmmm
} // namespace esphome
