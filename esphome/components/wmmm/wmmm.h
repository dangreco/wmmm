#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace wmmm {

class WmmmComponent : public Component, public uart::UARTDevice {
public:
  void dump_config() override;
  void setup() override;
  void loop() override;
};

} // namespace wmmm
} // namespace esphome
