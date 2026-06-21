#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#include "aup.hpp"

namespace esphome {
namespace unilux_uart {

class UniluxUartComponent : public Component, public uart::UARTDevice {
public:
  void dump_config() override;
  void setup() override;
  void loop() override;

private:
  unilux::aup::Decoder decoder_;
};

} // namespace unilux_uart
} // namespace esphome
