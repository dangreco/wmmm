#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#include "aup.hpp"
#include "wmmm.hpp"

namespace esphome {
namespace unilux_uart {

class UniluxUartComponent : public Component, public uart::UARTDevice {
public:
  void dump_config() override;
  void setup() override;
  void loop() override;

  void set_t1_sensor(sensor::Sensor *t1_sensor) {
    this->t1_sensor_ = t1_sensor;
  }
  void set_t2_sensor(sensor::Sensor *t2_sensor) {
    this->t2_sensor_ = t2_sensor;
  }

protected:
  /// Decode the WMMM message in @p frame, log it, and publish sensor states.
  void log_frame_(const unilux::aup::Frame &frame);

  unilux::aup::Decoder decoder_; ///< AUP byte-framing decoder.
  unilux::Decoder wmmm_decoder_; ///< WMMM message decoder.

  /// Temperature channels; nullptr when the channel is disabled in the config.
  sensor::Sensor *t1_sensor_{nullptr};
  sensor::Sensor *t2_sensor_{nullptr};
};

} // namespace unilux_uart
} // namespace esphome
