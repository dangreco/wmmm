#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace wmmm {

class WmmmComponent : public Component,
                      public uart::UARTDevice,
                      public sensor::Sensor {
public:
  void setup() override;
  void loop() override;
  void dump_config() override;
};

} // namespace wmmm
} // namespace esphome
