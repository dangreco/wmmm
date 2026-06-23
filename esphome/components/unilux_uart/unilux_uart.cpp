#include "unilux_uart.h"

#include "esphome/core/alloc_helpers.h"
#include "esphome/core/log.h"

#include "messages.hpp"

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace esphome {
namespace unilux_uart {

static const char *const TAG = "unilux_uart";

// AUP header is constant for this protocol; outgoing frames reuse these values
// (the decoder never verifies the checksum, so it is sent as zero).
static constexpr uint8_t AUP_TX_CHECKSUM = 0x00;
static constexpr uint8_t AUP_TX_FLAG = 0x00;
static constexpr uint8_t AUP_TX_TYPE = 0x01;
static constexpr uint8_t AUP_TX_COMMAND = 0x10;

// Setpoint used at startup until the device broadcasts its real target (0x2A),
// so the climate is controllable and never shows the ambient value as target.
static constexpr float DEFAULT_TARGET_TEMPERATURE = 20.0f;

// Map the device's HVAC mode (0x5C) to a Home Assistant climate mode. The
// device's "auto" is a heat-or-cool mode with a single adjustable setpoint, so
// it maps to HEAT_COOL (CLIMATE_MODE_AUTO would make the target unadjustable).
static optional<climate::ClimateMode>
device_mode_to_climate(unilux::message::Mode::Value value) {
  switch (value) {
  case unilux::message::Mode::Value::Heat:
    return climate::CLIMATE_MODE_HEAT;
  case unilux::message::Mode::Value::Cool:
    return climate::CLIMATE_MODE_COOL;
  case unilux::message::Mode::Value::Auto:
    return climate::CLIMATE_MODE_HEAT_COOL;
  }
  return {};
}

// Inverse of device_mode_to_climate for the supported modes.
static unilux::message::Mode::Value
climate_mode_to_device(climate::ClimateMode mode) {
  switch (mode) {
  case climate::CLIMATE_MODE_COOL:
    return unilux::message::Mode::Value::Cool;
  case climate::CLIMATE_MODE_HEAT_COOL:
    return unilux::message::Mode::Value::Auto;
  default:
    return unilux::message::Mode::Value::Heat;
  }
}

// Whether a climate mode is an active (powered-on) operating mode, i.e. one
// that maps to a device HVAC mode. OFF is handled via the Power message.
static bool is_active_mode(climate::ClimateMode mode) {
  return mode == climate::CLIMATE_MODE_HEAT ||
         mode == climate::CLIMATE_MODE_COOL ||
         mode == climate::CLIMATE_MODE_HEAT_COOL;
}

// Map the device's fan speed (0x22) to a Home Assistant climate fan mode.
static optional<climate::ClimateFanMode>
device_fan_to_climate(unilux::message::FanSpeed::Value value) {
  switch (value) {
  case unilux::message::FanSpeed::Value::Auto:
    return climate::CLIMATE_FAN_AUTO;
  case unilux::message::FanSpeed::Value::Low:
    return climate::CLIMATE_FAN_LOW;
  case unilux::message::FanSpeed::Value::Medium:
    return climate::CLIMATE_FAN_MEDIUM;
  case unilux::message::FanSpeed::Value::High:
    return climate::CLIMATE_FAN_HIGH;
  case unilux::message::FanSpeed::Value::Off:
    return climate::CLIMATE_FAN_OFF;
  }
  return {};
}

// Inverse of device_fan_to_climate for the supported fan modes.
static unilux::message::FanSpeed::Value
climate_fan_to_device(climate::ClimateFanMode mode) {
  switch (mode) {
  case climate::CLIMATE_FAN_LOW:
    return unilux::message::FanSpeed::Value::Low;
  case climate::CLIMATE_FAN_MEDIUM:
    return unilux::message::FanSpeed::Value::Medium;
  case climate::CLIMATE_FAN_HIGH:
    return unilux::message::FanSpeed::Value::High;
  case climate::CLIMATE_FAN_OFF:
    return unilux::message::FanSpeed::Value::Off;
  default:
    return unilux::message::FanSpeed::Value::Auto;
  }
}

void UniluxUartComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Unilux UART:");
  check_uart_settings(115200);
}

void UniluxUartComponent::setup() {}

void UniluxUartComponent::send_message(const unilux::Message &msg) {
  // Message -> WMMM frame -> WMMM bytes -> AUP frame -> AUP bytes -> UART.
  std::vector<uint8_t> wmmm_bytes = this->wmmm_encoder_.encode(msg.encode());

  unilux::aup::Frame aup;
  aup.checksum = AUP_TX_CHECKSUM;
  aup.flag = AUP_TX_FLAG;
  aup.type = AUP_TX_TYPE;
  aup.command = AUP_TX_COMMAND;
  aup.length = static_cast<uint16_t>(wmmm_bytes.size());
  aup.payload = std::move(wmmm_bytes);

  std::vector<uint8_t> out = this->aup_encoder_.encode(aup);
  this->write_array(out.data(), out.size());
}

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
              // Channel 1 drives the climate entity's current temperature.
              if (this->climate_ != nullptr)
                this->climate_->publish_current_temperature(m.t1);
            } else if constexpr (std::is_same_v<
                                     T, unilux::message::TargetTemperature>) {
              ESP_LOGD(TAG, "│ TargetTemperature t1=%.1f°C t2=%.1f°C",
                       static_cast<double>(m.t1), static_cast<double>(m.t2));
              // Channel 1 drives the climate entity's setpoint (reflecting
              // device state; this does not transmit).
              if (this->climate_ != nullptr)
                this->climate_->publish_target_temperature(m.t1);
            } else if constexpr (std::is_same_v<T, unilux::message::Mode>) {
              ESP_LOGD(TAG, "│ %s", m.to_string().c_str());
              // The HVAC mode drives the climate entity's mode (reflecting
              // device state; this does not transmit).
              if (this->climate_ != nullptr)
                this->climate_->publish_mode(m.value);
            } else if constexpr (std::is_same_v<T, unilux::message::Power>) {
              ESP_LOGD(TAG, "│ %s", m.to_string().c_str());
              // Power off shows as the OFF mode; on restores the active mode.
              if (this->climate_ != nullptr)
                this->climate_->publish_power(m.on);
            } else if constexpr (std::is_same_v<T, unilux::message::FanSpeed>) {
              ESP_LOGD(TAG, "│ %s", m.to_string().c_str());
              // The fan speed drives the climate entity's fan mode (reflecting
              // device state; this does not transmit).
              if (this->climate_ != nullptr)
                this->climate_->publish_fan_speed(m.value);
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

// --- UniluxUartClimate ------------------------------------------------------

void UniluxUartClimate::setup() {
  // Restore the last setpoint across reboots if one was saved.
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  }
  // Derive the power state and active mode from the restored HA mode; the
  // device's 0x21/0x5C broadcasts then set the real values.
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->power_on_ = false;
  } else if (is_active_mode(this->mode)) {
    this->power_on_ = true;
    this->active_mode_ = this->mode;
  } else {
    this->power_on_ = true;
    this->active_mode_ = climate::CLIMATE_MODE_HEAT;
  }
  this->mode = this->power_on_ ? this->active_mode_ : climate::CLIMATE_MODE_OFF;
  if (std::isnan(this->target_temperature)) {
    this->target_temperature = DEFAULT_TARGET_TEMPERATURE;
  }
  // Default the fan to AUTO until the device's 0x22 broadcast sets the real
  // one.
  if (!this->fan_mode.has_value()) {
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
  }
  this->publish_state();
}

void UniluxUartClimate::dump_config() {
  LOG_CLIMATE("", "Unilux UART Climate", this);
}

climate::ClimateTraits UniluxUartClimate::traits() {
  climate::ClimateTraits traits;
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  // Single-point (no two-point flag), so the setpoint is one adjustable value
  // in every mode. The device's "auto" is exposed as HEAT_COOL; OFF maps to the
  // power state (0x21).
  traits.set_supported_modes(
      {climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT,
       climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT_COOL});
  traits.set_supported_fan_modes(
      {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
       climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH,
       climate::CLIMATE_FAN_OFF});
  // Defaults; overridden by any `visual:` block in the YAML config.
  traits.set_visual_min_temperature(5.0f);
  traits.set_visual_max_temperature(40.0f);
  traits.set_visual_temperature_step(0.5f);
  return traits;
}

void UniluxUartClimate::control(const climate::ClimateCall &call) {
  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
    // Encode and transmit the new setpoint to the device. Only channel 1 is
    // meaningful; channel 2 is sent as zero.
    if (this->parent_ != nullptr) {
      this->parent_->send_message(
          unilux::message::TargetTemperature(this->target_temperature, 0.0f));
    }
  }
  if (call.get_mode().has_value()) {
    climate::ClimateMode requested = *call.get_mode();
    if (requested == climate::CLIMATE_MODE_OFF) {
      // Power off; the active mode is remembered for when it powers back on.
      if (this->power_on_ && this->parent_ != nullptr) {
        this->parent_->send_message(unilux::message::Power(false));
      }
      this->power_on_ = false;
    } else if (is_active_mode(requested)) {
      // Power on if needed, then set the HVAC mode if it changed.
      if (!this->power_on_ && this->parent_ != nullptr) {
        this->parent_->send_message(unilux::message::Power(true));
      }
      if (requested != this->active_mode_ && this->parent_ != nullptr) {
        this->parent_->send_message(
            unilux::message::Mode(climate_mode_to_device(requested)));
      }
      this->power_on_ = true;
      this->active_mode_ = requested;
    }
    this->mode =
        this->power_on_ ? this->active_mode_ : climate::CLIMATE_MODE_OFF;
  }
  if (call.get_fan_mode().has_value()) {
    this->fan_mode = *call.get_fan_mode();
    // Encode and transmit the new fan speed to the device.
    if (this->parent_ != nullptr) {
      this->parent_->send_message(
          unilux::message::FanSpeed(climate_fan_to_device(*this->fan_mode)));
    }
  }
  this->publish_state();
}

void UniluxUartClimate::publish_current_temperature(float temperature) {
  this->current_temperature = temperature;
  this->publish_state();
}

void UniluxUartClimate::publish_target_temperature(float temperature) {
  this->target_temperature = temperature;
  this->publish_state();
}

void UniluxUartClimate::publish_mode(unilux::message::Mode::Value value) {
  auto mode = device_mode_to_climate(value);
  if (!mode.has_value()) {
    ESP_LOGW(TAG, "Unknown HVAC mode 0x%02X", static_cast<unsigned>(value));
    return;
  }
  this->active_mode_ = *mode;
  this->publish_combined_mode_();
}

void UniluxUartClimate::publish_power(bool on) {
  this->power_on_ = on;
  this->publish_combined_mode_();
}

void UniluxUartClimate::publish_fan_speed(
    unilux::message::FanSpeed::Value value) {
  auto fan_mode = device_fan_to_climate(value);
  if (!fan_mode.has_value()) {
    ESP_LOGW(TAG, "Unknown fan speed 0x%02X", static_cast<unsigned>(value));
    return;
  }
  this->fan_mode = *fan_mode;
  this->publish_state();
}

void UniluxUartClimate::publish_combined_mode_() {
  this->mode = this->power_on_ ? this->active_mode_ : climate::CLIMATE_MODE_OFF;
  this->publish_state();
}

} // namespace unilux_uart
} // namespace esphome
