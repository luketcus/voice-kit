#pragma once

#include "esphome/components/http_request/http_request.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "../voice_kit.h"

#include <memory>
#include <string>
#include <utility>

namespace esphome {
namespace voice_kit {

static const uint8_t MD5_SIZE = 32;
static const uint8_t DFU_CONTROLLER_SERVICER_RESID = 240;
static const uint8_t DFU_COMMAND_READ_BIT = 0x80;
static const uint16_t DFU_TIMEOUT_MS = 1000;

enum OtaVoiceKitComponentError : uint8_t {
  OTA_MD5_INVALID = 0x10,
  OTA_BAD_URL = 0x11,
  OTA_CONNECTION_ERROR = 0x12,
  OTA_COMMUNICATION_ERROR = 0x20,
};

enum TransportProtocolReturnCode : uint8_t {
  CTRL_DONE = 0,
  CTRL_WAIT = 1,
  CTRL_INVALID = 3,
};

// DFU enums from https://github.com/xmos/sln_voice/blob/develop/examples/ffva/src/dfu_int/dfu_state_machine.h

enum DfuIntAltSetting : uint8_t {
  DFU_INT_ALTERNATE_FACTORY,
  DFU_INT_ALTERNATE_UPGRADE,
};

enum DfuIntState : uint8_t {
  DFU_INT_APP_IDLE,    // unused
  DFU_INT_APP_DETACH,  // unused
  DFU_INT_DFU_IDLE,
  DFU_INT_DFU_DNLOAD_SYNC,
  DFU_INT_DFU_DNBUSY,
  DFU_INT_DFU_DNLOAD_IDLE,
  DFU_INT_DFU_MANIFEST_SYNC,
  DFU_INT_DFU_MANIFEST,
  DFU_INT_DFU_MANIFEST_WAIT_RESET,
  DFU_INT_DFU_UPLOAD_IDLE,
  DFU_INT_DFU_ERROR,
};

enum DfuIntStatus : uint8_t {
  DFU_INT_DFU_STATUS_OK,
  DFU_INT_DFU_STATUS_ERR_TARGET,
  DFU_INT_DFU_STATUS_ERR_FILE,
  DFU_INT_DFU_STATUS_ERR_WRITE,
  DFU_INT_DFU_STATUS_ERR_ERASE,
  DFU_INT_DFU_STATUS_ERR_CHECK_ERASED,
  DFU_INT_DFU_STATUS_ERR_PROG,
  DFU_INT_DFU_STATUS_ERR_VERIFY,
  DFU_INT_DFU_STATUS_ERR_ADDRESS,
  DFU_INT_DFU_STATUS_ERR_NOTDONE,
  DFU_INT_DFU_STATUS_ERR_FIRMWARE,
  DFU_INT_DFU_STATUS_ERR_VENDOR,
  DFU_INT_DFU_STATUS_ERR_USBR,
  DFU_INT_DFU_STATUS_ERR_POR,
  DFU_INT_DFU_STATUS_ERR_UNKNOWN,
  DFU_INT_DFU_STATUS_ERR_STALLEDPKT,
};

enum DfuCommands : uint8_t {
  DFU_CONTROLLER_SERVICER_RESID_DFU_DETACH = 0,
  DFU_CONTROLLER_SERVICER_RESID_DFU_DNLOAD = 1,
  DFU_CONTROLLER_SERVICER_RESID_DFU_UPLOAD = 2,
  DFU_CONTROLLER_SERVICER_RESID_DFU_GETSTATUS = 3,
  DFU_CONTROLLER_SERVICER_RESID_DFU_CLRSTATUS = 4,
  DFU_CONTROLLER_SERVICER_RESID_DFU_GETSTATE = 5,
  DFU_CONTROLLER_SERVICER_RESID_DFU_ABORT = 6,
  DFU_CONTROLLER_SERVICER_RESID_DFU_SETALTERNATE = 64,
  DFU_CONTROLLER_SERVICER_RESID_DFU_TRANSFERBLOCK = 65,
  DFU_CONTROLLER_SERVICER_RESID_DFU_GETVERSION = 88,
  DFU_CONTROLLER_SERVICER_RESID_DFU_REBOOT = 89,
};

class OtaVoiceKitComponent : public ota::OTAComponent, public Parented<http_request::HttpRequestComponent> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_voice_kit(VoiceKit *voice_kit_component) { this->voice_kit_component_ = voice_kit_component; }
  void set_md5_url(const std::string &md5_url);
  void set_md5(const std::string &md5) { this->md5_expected_ = md5; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_url(const std::string &url);
  void set_username(const std::string &username) { this->username_ = username; }

  std::string md5_computed() { return this->md5_computed_; }
  std::string md5_expected() { return this->md5_expected_; }

  void flash();

 protected:
  void cleanup_(const std::shared_ptr<http_request::HttpContainer> &container);
  uint8_t do_ota_();
  std::string get_url_with_auth_(const std::string &url);
  bool http_get_md5_();
  bool validate_url_(const std::string &url);

  bool dfu_get_status_();
  bool dfu_get_version_();
  bool dfu_reboot_();
  bool dfu_set_alternate_();
  bool dfu_wait_for_idle_();

  uint8_t dfu_state_{0};
  uint8_t dfu_status_{0};
  uint32_t dfu_status_next_req_delay_{0};

  VoiceKit *voice_kit_component_{nullptr};

  std::string md5_computed_{};
  std::string md5_expected_{};
  std::string md5_url_{};
  std::string password_{};
  std::string username_{};
  std::string url_{};
  static const uint16_t HTTP_RECV_BUFFER = 128;  // the firmware GET chunk size
};

}  // namespace voice_kit
}  // namespace esphome
