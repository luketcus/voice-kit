#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "voice_kit.h"

namespace esphome {
namespace voice_kit {

#ifdef USE_AUDIO_PROCESSOR
template<typename... Ts> class SetVnrValueAction : public Action<Ts...> {
 public:
  explicit SetVnrValueAction(VoiceKit *voice_kit) : voice_kit_(voice_kit) {}

  TEMPLATABLE_VALUE(uint8_t, vnr_value)

  void play(Ts... x) override { this->voice_kit_->set_vnr_value(this->vnr_value_.optional_value(x...)); }

 protected:
  VoiceKit *voice_kit_;
};
#endif

}  // namespace voice_kit
}  // namespace esphome
