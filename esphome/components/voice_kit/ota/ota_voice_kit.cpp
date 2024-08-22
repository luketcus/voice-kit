#include "ota_voice_kit.h"

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esphome/components/md5/md5.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/watchdog/watchdog.h"

#include <cinttypes>

namespace esphome {
namespace voice_kit {

static const char *const TAG = "voice_kit.ota";

void OtaVoiceKitComponent::setup() {
#ifdef USE_OTA_STATE_CALLBACK
  ota::register_ota_platform(this);
#endif
}

void OtaVoiceKitComponent::dump_config() { ESP_LOGCONFIG(TAG, "Over-The-Air updates via HTTP request for XMOS SoCs"); };

void OtaVoiceKitComponent::set_md5_url(const std::string &url) {
  if (!this->validate_url_(url)) {
    this->md5_url_.clear();  // URL was not valid; prevent flashing until it is
    return;
  }
  this->md5_url_ = url;
  this->md5_expected_.clear();  // to be retrieved later
}

void OtaVoiceKitComponent::set_url(const std::string &url) {
  if (!this->validate_url_(url)) {
    this->url_.clear();  // URL was not valid; prevent flashing until it is
    return;
  }
  this->url_ = url;
}

void OtaVoiceKitComponent::flash() {
  if (this->url_.empty()) {
    ESP_LOGE(TAG, "URL not set; cannot start update");
    return;
  }

  ESP_LOGI(TAG, "Starting update...");
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(ota::OTA_STARTED, 0.0f, 0);
#endif

  auto ota_status = this->do_ota_();

  switch (ota_status) {
    case ota::OTA_RESPONSE_OK:
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(ota::OTA_COMPLETED, 100.0f, ota_status);
#endif
      // delay(10);
      // App.safe_reboot();
      break;

    default:
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(ota::OTA_ERROR, 0.0f, ota_status);
#endif
      this->md5_computed_.clear();  // will be reset at next attempt
      this->md5_expected_.clear();  // will be reset at next attempt
      break;
  }
}

void OtaVoiceKitComponent::cleanup_(const std::shared_ptr<http_request::HttpContainer> &container) {
  ESP_LOGV(TAG, "Aborting HTTP connection");
  container->end();
};

uint8_t OtaVoiceKitComponent::do_ota_() {
  uint32_t last_progress = 0;
  uint32_t update_start_time = millis();
  md5::MD5Digest md5_receive;
  std::unique_ptr<char[]> md5_receive_str(new char[33]);

  if (this->md5_expected_.empty() && !this->http_get_md5_()) {
    return OTA_MD5_INVALID;
  }

  ESP_LOGD(TAG, "MD5 expected: %s", this->md5_expected_.c_str());

  auto url_with_auth = this->get_url_with_auth_(this->url_);
  if (url_with_auth.empty()) {
    return OTA_BAD_URL;
  }

  // we will compute MD5 on the fly for verification
  md5_receive.init();
  ESP_LOGV(TAG, "MD5Digest initialized");

  if (!this->dfu_get_version_()) {
    ESP_LOGE(TAG, "Initial version check failed");
    return OTA_COMMUNICATION_ERROR;
  }

  if (!this->dfu_set_alternate_()) {
    ESP_LOGE(TAG, "Set alternate request failed");
    return OTA_COMMUNICATION_ERROR;
  }

  ESP_LOGVV(TAG, "url_with_auth: %s", url_with_auth.c_str());
  ESP_LOGI(TAG, "Connecting to: %s", this->url_.c_str());

  auto container = this->parent_->get(url_with_auth);

  if (container == nullptr) {
    return OTA_CONNECTION_ERROR;
  }

  ESP_LOGV(TAG, "OTA begin");

  uint8_t dfu_dnload_req[HTTP_RECV_BUFFER + 6] = {240, 1, 130,  // resid, cmd_id, payload length,
                                                  0, 0};        // additional payload length (set below)
                                                                // followed by payload data with null terminator
  while (container->get_bytes_read() < container->content_length) {
    if (!this->dfu_wait_for_idle_()) {
      return OTA_COMMUNICATION_ERROR;
    }

    // read a maximum of chunk_size bytes into buf. (real read size returned)
    int bufsize = container->read(&dfu_dnload_req[5], HTTP_RECV_BUFFER);
    ESP_LOGVV(TAG, "bytes_read_ = %u, body_length_ = %u, bufsize = %i", container->get_bytes_read(),
              container->content_length, bufsize);

    if (bufsize < 0) {
      ESP_LOGE(TAG, "Stream closed");
      this->cleanup_(container);
      return OTA_CONNECTION_ERROR;
    } else if (bufsize > 0 && bufsize <= HTTP_RECV_BUFFER) {
      // add read bytes to MD5
      md5_receive.add(&dfu_dnload_req[5], bufsize);

      // write bytes to XMOS
      dfu_dnload_req[3] = (uint8_t) bufsize;
      auto error_code = this->voice_kit_component_->write(dfu_dnload_req, sizeof(dfu_dnload_req) - 1);
      if (error_code != i2c::ERROR_OK) {
        ESP_LOGE(TAG, "DFU download request failed");
        this->cleanup_(container);
        return OTA_COMMUNICATION_ERROR;
      }
    }

    uint32_t now = millis();
    if ((now - last_progress > 1000) or (container->get_bytes_read() == container->content_length)) {
      last_progress = now;
      float percentage = container->get_bytes_read() * 100.0f / container->content_length;
      ESP_LOGD(TAG, "Progress: %0.1f%%", percentage);
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(ota::OTA_IN_PROGRESS, percentage, 0);
#endif
    }
  }  // while

  if (!this->dfu_wait_for_idle_()) {
    return OTA_COMMUNICATION_ERROR;
  }

  memset(&dfu_dnload_req[3], 0, HTTP_RECV_BUFFER + 2);
  // send empty download request to conclude DFU download
  auto error_code = this->voice_kit_component_->write(dfu_dnload_req, sizeof(dfu_dnload_req) - 1);
  if (error_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Final DFU download request failed");
    this->cleanup_(container);
    return OTA_COMMUNICATION_ERROR;
  }

  container->end();

  if (!this->dfu_wait_for_idle_()) {
    return OTA_COMMUNICATION_ERROR;
  }

  ESP_LOGI(TAG, "Done in %.0f seconds", float(millis() - update_start_time) / 1000);

  // verify MD5 is as expected and act accordingly
  md5_receive.calculate();
  md5_receive.get_hex(md5_receive_str.get());
  this->md5_computed_ = md5_receive_str.get();
  if (strncmp(this->md5_computed_.c_str(), this->md5_expected_.c_str(), MD5_SIZE) != 0) {
    ESP_LOGE(TAG, "MD5 computed: %s - Aborting due to MD5 mismatch", this->md5_computed_.c_str());
    return ota::OTA_RESPONSE_ERROR_MD5_MISMATCH;
  }

  ESP_LOGI(TAG, "Rebooting XMOS SoC...");
  if (!this->dfu_reboot_()) {
    return OTA_COMMUNICATION_ERROR;
  }

  delay(100);  // NOLINT

  while (!this->dfu_get_version_()) {
    delay(250);  // NOLINT
    // feed watchdog and give other tasks a chance to run
    App.feed_wdt();
    yield();
  }

  ESP_LOGI(TAG, "Update complete");

  return ota::OTA_RESPONSE_OK;
}

std::string OtaVoiceKitComponent::get_url_with_auth_(const std::string &url) {
  if (this->username_.empty() || this->password_.empty()) {
    return url;
  }

  auto start_char = url.find("://");
  if ((start_char == std::string::npos) || (start_char < 4)) {
    ESP_LOGE(TAG, "Incorrect URL prefix");
    return {};
  }

  ESP_LOGD(TAG, "Using basic HTTP authentication");

  start_char += 3;  // skip '://' characters
  auto url_with_auth =
      url.substr(0, start_char) + this->username_ + ":" + this->password_ + "@" + url.substr(start_char);
  return url_with_auth;
}

bool OtaVoiceKitComponent::http_get_md5_() {
  if (this->md5_url_.empty()) {
    return false;
  }

  auto url_with_auth = this->get_url_with_auth_(this->md5_url_);
  if (url_with_auth.empty()) {
    return false;
  }

  ESP_LOGVV(TAG, "url_with_auth: %s", url_with_auth.c_str());
  ESP_LOGI(TAG, "Connecting to: %s", this->md5_url_.c_str());
  auto container = this->parent_->get(url_with_auth);
  if (container == nullptr) {
    ESP_LOGE(TAG, "Failed to connect to MD5 URL");
    return false;
  }
  size_t length = container->content_length;
  if (length == 0) {
    container->end();
    return false;
  }
  if (length < MD5_SIZE) {
    ESP_LOGE(TAG, "MD5 file must be %u bytes; %u bytes reported by HTTP server. Aborting", MD5_SIZE, length);
    container->end();
    return false;
  }

  this->md5_expected_.resize(MD5_SIZE);
  int read_len = 0;
  while (container->get_bytes_read() < MD5_SIZE) {
    read_len = container->read((uint8_t *) this->md5_expected_.data(), MD5_SIZE);
    App.feed_wdt();
    yield();
  }
  container->end();

  ESP_LOGV(TAG, "Read len: %u, MD5 expected: %u", read_len, MD5_SIZE);
  return read_len == MD5_SIZE;
}

bool OtaVoiceKitComponent::validate_url_(const std::string &url) {
  if ((url.length() < 8) || (url.find("http") != 0) || (url.find("://") == std::string::npos)) {
    ESP_LOGE(TAG, "URL is invalid and/or must be prefixed with 'http://' or 'https://'");
    return false;
  }
  return true;
}

bool OtaVoiceKitComponent::dfu_get_status_() {
  const uint8_t status_req[] = {DFU_CONTROLLER_SERVICER_RESID,
                                DFU_CONTROLLER_SERVICER_RESID_DFU_GETSTATUS | DFU_COMMAND_READ_BIT, 6};
  uint8_t status_resp[6];

  auto error_code = this->voice_kit_component_->write(status_req, sizeof(status_req));
  if (error_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Request status failed");
    return false;
  }

  error_code = this->voice_kit_component_->read(status_resp, sizeof(status_resp));
  if (error_code != i2c::ERROR_OK || status_resp[0] != CTRL_DONE) {
    ESP_LOGE(TAG, "Read status failed");
    return false;
  }
  this->dfu_status_next_req_delay_ = encode_uint24(status_resp[4], status_resp[3], status_resp[2]);
  this->dfu_state_ = status_resp[5];
  this->dfu_status_ = status_resp[1];
  ESP_LOGVV(TAG, "status_resp: %u %u - %ums", status_resp[1], status_resp[5], this->dfu_status_next_req_delay_);
  return true;
}

bool OtaVoiceKitComponent::dfu_get_version_() {
  const uint8_t version_req[] = {DFU_CONTROLLER_SERVICER_RESID,
                                 DFU_CONTROLLER_SERVICER_RESID_DFU_GETVERSION | DFU_COMMAND_READ_BIT, 4};
  uint8_t version_resp[4];

  auto error_code = this->voice_kit_component_->write(version_req, sizeof(version_req));
  if (error_code != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Request version failed");
    return false;
  }

  error_code = this->voice_kit_component_->read(version_resp, sizeof(version_resp));
  if (error_code != i2c::ERROR_OK || version_resp[0] != CTRL_DONE) {
    ESP_LOGW(TAG, "Read version failed");
    return false;
  }

  ESP_LOGI(TAG, "DFU version: %u.%u.%u", version_resp[1], version_resp[2], version_resp[3]);
  return true;
}

bool OtaVoiceKitComponent::dfu_reboot_() {
  const uint8_t reboot_req[] = {DFU_CONTROLLER_SERVICER_RESID, DFU_CONTROLLER_SERVICER_RESID_DFU_REBOOT, 1};
  auto error_code = this->voice_kit_component_->write(reboot_req, 4);
  if (error_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Reboot request failed");
    return false;
  }
  return true;
}

bool OtaVoiceKitComponent::dfu_set_alternate_() {
  const uint8_t setalternate_req[] = {DFU_CONTROLLER_SERVICER_RESID, DFU_CONTROLLER_SERVICER_RESID_DFU_SETALTERNATE, 1,
                                      DFU_INT_ALTERNATE_UPGRADE};  // resid, cmd_id, payload length, payload data
  auto error_code = this->voice_kit_component_->write(setalternate_req, sizeof(setalternate_req));
  if (error_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "SetAlternate request failed");
    return false;
  }
  return true;
}

bool OtaVoiceKitComponent::dfu_wait_for_idle_() {
  auto wait_for_idle_start_ms = millis();
  auto status_last_read_ms = millis();
  this->dfu_status_next_req_delay_ = 0;  // clear this, it'll be refreshed below

  do {
    if (millis() > status_last_read_ms + this->dfu_status_next_req_delay_) {
      if (!this->dfu_get_status_()) {
        return false;
      }
      status_last_read_ms = millis();
      ESP_LOGVV(TAG, "DFU state: %u, status: %u, delay: %" PRIu32, this->dfu_state_, this->dfu_status_,
                this->dfu_status_next_req_delay_);

      if ((this->dfu_state_ == DFU_INT_DFU_IDLE) || (this->dfu_state_ == DFU_INT_DFU_DNLOAD_IDLE) ||
          (this->dfu_state_ == DFU_INT_DFU_MANIFEST_WAIT_RESET)) {
        return true;
      }
    }
    // feed watchdog and give other tasks a chance to run
    App.feed_wdt();
    yield();
  } while (wait_for_idle_start_ms + DFU_TIMEOUT_MS > millis());

  ESP_LOGE(TAG, "Timeout waiting for DFU idle state");
  return false;
}

}  // namespace voice_kit
}  // namespace esphome
