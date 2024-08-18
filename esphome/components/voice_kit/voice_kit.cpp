#include "voice_kit.h"

#include "esphome/core/defines.h"
#include "esphome/core/log.h"

namespace esphome {
namespace voice_kit {

static const char *const TAG = "voice_kit";

void VoiceKit::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Voice Kit...");

  // Check to see if the device can read from a register
  // uint8_t port = 0;
  // if (this->read_register(0x0, &port, 1) != i2c::ERROR_OK) {
  //   this->mark_failed();
  //   return;
  // }
}

void VoiceKit::dump_config() {
  ESP_LOGCONFIG(TAG, "Voice Kit:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with Voice Kit failed");
  }
}

#ifdef USE_AUDIO_PROCESSOR
// void VoiceKit::set_vnr_value(optional<uint8_t> vnr_value) {
//   // actually doing read here because testing stuff
//   uint8_t read_buffer[10];
//   uint8_t read_vnr[] = {
//       CONFIGURATION_SERVICER_RESID,
//       CONFIGURATION_SERVICER_RESID_VNR_VALUE,
//       7,
//   };

//   if (!vnr_value.has_value() || this->write(read_vnr, sizeof(read_vnr), false) != i2c::ERROR_OK) {
//     ESP_LOGW(TAG, "Couldn't request VNR value");
//   } else {
//     ESP_LOGD(TAG, "Reading VNR value...");
//   }

//   if (this->read(read_buffer, 10) != i2c::ERROR_OK) {
//     ESP_LOGW(TAG, "Couldn't read VNR value");
//   } else {
//     ESP_LOGD(TAG, "VNR value: %u %u %u %u %u %u %u %u %u %u", read_buffer[0], read_buffer[1], read_buffer[2],
//              read_buffer[3], read_buffer[4], read_buffer[5], read_buffer[6], read_buffer[7], read_buffer[8],
//              read_buffer[9]);
//   }
// }

void VoiceKit::set_vnr_value(optional<uint8_t> vnr_value) {
  uint8_t set_vnr[] = {
      CONFIGURATION_SERVICER_RESID,
      CONFIGURATION_SERVICER_RESID_VNR_VALUE,
      // CONFIGURATION_SERVICER_RESID_LINEOUT_VOLUME,
      1,
      vnr_value.value_or(0),
  };

  if (!vnr_value.has_value() || this->write(set_vnr, sizeof(set_vnr)) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "VNR value not set");
  } else {
    ESP_LOGD(TAG, "Set VNR value to %u", vnr_value.value());
  }
}
#endif

}  // namespace voice_kit
}  // namespace esphome
