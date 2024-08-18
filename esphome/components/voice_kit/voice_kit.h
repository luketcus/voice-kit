#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace voice_kit {

// Configuration servicer metadata
//
static const uint8_t CONFIGURATION_SERVICER_HEADER = 0xA5;
static const uint8_t CONFIGURATION_SERVICER_CONTROL_VERSION = 0x10;

// Configuration servicer resource IDs
//
static const uint8_t CONFIGURATION_SERVICER_RESID = 241;

// Configuration servicer command IDs
//
static const uint8_t CONFIGURATION_SERVICER_RESID_VNR_VALUE = 0x00;
static const uint8_t CONFIGURATION_SERVICER_RESID_LINEOUT_VOLUME = 0x10;
static const uint8_t CONFIGURATION_SERVICER_RESID_HEADPHONE_VOLUME = 0x11;

class VoiceKit : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

#ifdef USE_AUDIO_PROCESSOR
  void set_vnr_value(optional<uint8_t> vnr_value);
#endif
};

}  // namespace voice_kit
}  // namespace esphome