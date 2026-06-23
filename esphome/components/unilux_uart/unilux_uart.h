#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#include "aup.hpp"
#include "message.hpp"
#include "messages/mode.hpp"
#include "wmmm.hpp"

namespace esphome {
namespace unilux_uart {

class UniluxUartClimate;

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
  void set_climate(UniluxUartClimate *climate) { this->climate_ = climate; }

  /// Serialise @p msg through the WMMM and AUP encoders and write it out the
  /// UART, using this protocol's constant AUP header.
  void send_message(const unilux::Message &msg);

protected:
  /// Decode the WMMM message in @p frame, log it, and publish entity states.
  void log_frame_(const unilux::aup::Frame &frame);

  unilux::aup::Decoder decoder_;     ///< AUP byte-framing decoder.
  unilux::Decoder wmmm_decoder_;     ///< WMMM message decoder.
  unilux::Encoder wmmm_encoder_;     ///< WMMM message encoder (TX).
  unilux::aup::Encoder aup_encoder_; ///< AUP byte-framing encoder (TX).

  /// Temperature channels; nullptr when the channel is disabled in the config.
  sensor::Sensor *t1_sensor_{nullptr};
  sensor::Sensor *t2_sensor_{nullptr};

  /// Target-temperature climate entity; nullptr when not configured.
  UniluxUartClimate *climate_{nullptr};
};

/// @brief Two-way target-temperature control surfaced as an ESPHome climate.
///
/// A received TargetTemperature (WMMM id 0x2A) frame updates this entity's
/// setpoint; changing the setpoint in Home Assistant encodes and transmits a
/// TargetTemperature frame back to the device via the parent hub.
class UniluxUartClimate : public climate::Climate, public Component {
public:
  void set_parent(UniluxUartComponent *parent) { this->parent_ = parent; }

  void setup() override;
  void dump_config() override;

  /// Update the displayed current temperature (driven by the Temperature
  /// message's first channel) and republish.
  void publish_current_temperature(float temperature);
  /// Update the target temperature from a received frame and republish (does
  /// not transmit; this reflects device state).
  void publish_target_temperature(float temperature);
  /// Update the HVAC mode from a received Mode frame and republish (does not
  /// transmit; this reflects device state). Unknown values are ignored.
  void publish_mode(unilux::message::Mode::Value value);
  /// Update the power state from a received Power frame and republish (does not
  /// transmit; this reflects device state).
  void publish_power(bool on);

protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  /// Recompute the HA mode from power + active mode and publish it.
  void publish_combined_mode_();

  UniluxUartComponent *parent_{nullptr};

  /// Power (0x21) and HVAC mode (0x5C) arrive as separate messages; HA folds
  /// them into one mode enum (OFF when powered off, else the active mode).
  bool power_on_{true};
  climate::ClimateMode active_mode_{climate::CLIMATE_MODE_HEAT};
};

} // namespace unilux_uart
} // namespace esphome
