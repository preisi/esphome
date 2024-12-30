#include "cypress_touch.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cypress_touch {

static const char *const TAG = "cypress_touch";

static const uint8_t CYPRESS_TOUCH_BASE_ADDR = 0x00;
static const uint8_t CYPRESS_TOUCH_SOFT_RST_MODE = 0x01;
static const uint8_t CYPRESS_TOUCH_SYSINFO_MODE = 0x10;
static const uint8_t CYPRESS_TOUCH_OPERATE_MODE = 0x00;
static const uint8_t CYPRESS_TOUCH_LOW_POWER_MODE = 0x04;
static const uint8_t CYPRESS_TOUCH_DEEP_SLEEP_MODE = 0x02;
static const uint8_t CYPRESS_TOUCH_REG_ACT_INTRVL = 0x1D;

static const uint8_t CYPRESS_TOUCH_ACT_INTRVL_DFTL = 0x00;
static const uint8_t CYPRESS_TOUCH_TCH_TMOUT_DFTL = 0xFF;
static const uint8_t CYPRESS_TOUCH_LP_INTRVL_DFTL = 0x0A;

float CypressTouchscreen::get_setup_priority() const { return setup_priority::HARDWARE_LATE - 1.0f; }

void CypressTouchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Cypress Touchscreen...");

  this->rts_pin_->setup();
  this->enable_pin_->setup();

  this->set_power(true);

  this->hard_reset_();

  ESP_LOGV(TAG, "Pinging Cypress touchscreen...");
  if (!this->ping_touchscreen(5)) {
    ESP_LOGE(TAG, "Failed to ping Cypress Touchscreen!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Soft-resetting touchscreen...");
  if (!this->soft_reset_()) {
    ESP_LOGE(TAG, "Failed to soft reset Cypress!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }

  // XXX: is this even necessary?
  ESP_LOGI(TAG, "Loading bootloader registers...");
  this->load_bootloader_registers(&this->bootloader_data_);

  if (!this->exit_bootloader_mode()) {
    ESP_LOGE(TAG, "Failed to exit Cypress bootloader mode!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Setting sysinfo mode...");
  if (!this->set_sysinfo_mode(&this->sysinfo_data_)) {
    ESP_LOGE(TAG, "Setting systeminfo mode for Cypress failed!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Setting sysinfo registers...");
  if (!this->set_sysinfo_registers(&this->sysinfo_data_)) {
    ESP_LOGE(TAG, "Setting systeminfo registers for Cypress failed!");
    this->interrupt_pin_->detach_interrupt();
    this->mark_failed();
    return;
  }
  uint8_t cmds[] = {CYPRESS_TOUCH_BASE_ADDR, CYPRESS_TOUCH_OPERATE_MODE};
  this->write(cmds, sizeof(cmds));

  uint8_t distDefaultValue = 0xF8;
  this->write_registers(0x1E, &distDefaultValue, 1);

  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT);
  this->interrupt_pin_->setup();
  // XXX: only attach interrupt once during initialization?
  this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);

  delay(50);

  // Configure esphome touchscreen for correct normalization of raw data
  this->x_raw_max_ = 682;
  this->y_raw_max_ = 1023;
  this->invert_y_ = true;
  this->swap_x_y_ = true;
}

void CypressTouchscreen::set_power(bool enable) {
  if (enable) {
    this->enable_pin_->digital_write(true);
    delay(50);
    this->rts_pin_->digital_write(true);
    delay(50);

  } else {
    this->enable_pin_->digital_write(false);
    delay(50);
    this->rts_pin_->digital_write(false);
  }
}

bool CypressTouchscreen::ping_touchscreen(int retries) {
  auto ret = i2c::ERROR_NOT_ACKNOWLEDGED;
  for (int i = 0; i < retries; ++i) {
    ret = this->write(NULL, 0);
    if (ret == i2c::ERROR_OK) {
      return true;
    }
    delay(20);
  }
  return false;
}

bool CypressTouchscreen::write_registers(uint8_t cmd, uint8_t *buffer, int len) {
  size_t bufSize = sizeof(uint8_t) * (len + 1);
  uint8_t buf[bufSize];
  buf[0] = cmd;
  memcpy(buf + 1, buffer, len);

  return this->write(buf, bufSize) == i2c::ERROR_OK;
}

bool CypressTouchscreen::read_registers(uint8_t cmd, uint8_t *buffer, int len) {
  if (this->write(&cmd, 1) != i2c::ERROR_OK) {
    return false;
  }
  int offset = 0;
  while (len > 0) {
    int i2cLen = len > 32 ? 32 : len;
    this->read(buffer + offset, i2cLen);
    offset += i2cLen;
    len -= i2cLen;
  }
  return true;
}

bool CypressTouchscreen::load_bootloader_registers(CypressTouchscreen::bootloader_data_t *blData) {
  uint8_t data[sizeof(CypressTouchscreen::bootloader_data_t)];
  if (!this->read_registers(CYPRESS_TOUCH_BASE_ADDR, data, sizeof(data))) {
    return false;
  }
  memcpy(blData, data, 16);
  return true;
}

bool CypressTouchscreen::exit_bootloader_mode() {
  uint8_t cmds[] = {
    0x00,                  // File offset
    0xFF,                  // Command
    0xA5,                  // Exit bootloader command
    0, 1, 2, 3, 4, 5, 6, 7 // Default keys
  };

  this->write_registers(CYPRESS_TOUCH_BASE_ADDR, cmds, sizeof(cmds));

  delay(500);

  CypressTouchscreen::bootloader_data_t blData;
  if (this->load_bootloader_registers(&blData)) {
    return false;
  }

  // Check for validity
  return (blData.bl_status & 0x10) >> 4;
}

bool CypressTouchscreen::set_sysinfo_mode(CypressTouchscreen::sysinfo_data_t *data) {
  uint8_t cmds[] = {CYPRESS_TOUCH_BASE_ADDR, CYPRESS_TOUCH_SYSINFO_MODE};
  this->write(cmds, 2);
  delay(20);
  uint8_t buffer[sizeof(CypressTouchscreen::sysinfo_data_t)];
  if (!this->read_registers(CYPRESS_TOUCH_BASE_ADDR, buffer, sizeof(buffer))) {
    return false;
  }
  memcpy(data, buffer, sizeof(buffer));
  this->handshake();
  if (!data->tts_verh && !data->tts_verl) {
    return false;
  }
  return true;
}

bool CypressTouchscreen::set_sysinfo_registers(sysinfo_data_t *data) {
  data->act_intrvl = CYPRESS_TOUCH_ACT_INTRVL_DFTL;
  data->tch_tmout = CYPRESS_TOUCH_TCH_TMOUT_DFTL;
  data->lp_intrvl = CYPRESS_TOUCH_LP_INTRVL_DFTL;

  uint8_t regs[] = {data->act_intrvl, data->tch_tmout, data->lp_intrvl};
  if (!this->write_registers(0x1D, regs, sizeof(regs))) {
    return false;
  }
  delay(20);
  return true;
}

void CypressTouchscreen::handshake() {
  uint8_t hstModeReg = 0;
  this->read_registers(CYPRESS_TOUCH_BASE_ADDR, &hstModeReg, 1);
  hstModeReg ^= 0x80;
  this->write_registers(CYPRESS_TOUCH_BASE_ADDR, &hstModeReg, 1);
}

bool CypressTouchscreen::get_touch_data(CypressTouchscreen::touch_data_t *report) {
  if (report == NULL) {
    return false;
  }

  memset(report, 0, sizeof(CypressTouchscreen::touch_data_t));

  uint8_t regs[sizeof(CypressTouchscreen::sysinfo_data_t)];
  if (!this->read_registers(CYPRESS_TOUCH_BASE_ADDR, regs, sizeof(regs))) {
    return false;
  }
  this->handshake();
  report->x[0] = regs[3] << 8 | regs[4];
  report->y[0] = regs[5] << 8 | regs[6];
  report->z[0] = regs[7];
  report->x[1] = regs[9] << 8 | regs[10];
  report->y[1] = regs[11] << 8 | regs[12];
  report->z[1] = regs[13];
  report->detectionType = regs[8];
  report->fingers = regs[2];

  return true;
}

// equivalent to Touch::tsGetData
void CypressTouchscreen::update_touches() {
  CypressTouchscreen::touch_data_t touchReport;

  if (!this->get_touch_data(&touchReport)) {
    return;
  }

  if (touchReport.fingers != 1 && touchReport.fingers != 2) {
    // This shouldn't happen, let's return for now..
    ESP_LOGW(TAG, "Invalid number of touches detected, ignoring...");
    return;
  };

  // XXX: rotation?

  ESP_LOGV(TAG, "Touch count: %d", touchReport.fingers);
  for (int i = 0; i < touchReport.fingers; ++i) {
    this->add_raw_touch_position_(i, touchReport.x[i], touchReport.y[i]);
  }
}

void CypressTouchscreen::set_power_state(bool enable) {
  // tp: see get_power_state... (currently, we're simply ignoring deep sleep)
  uint8_t data[] = {CYPRESS_TOUCH_BASE_ADDR, CYPRESS_TOUCH_LOW_POWER_MODE};
  if (enable)
    data[1] = CYPRESS_TOUCH_OPERATE_MODE;
  this->write(data, 2);
}

bool CypressTouchscreen::get_power_state() {
  // tp: somewhat done...
  uint8_t state;
  this->write(&CYPRESS_TOUCH_BASE_ADDR, 1);
  this->read(&state, 1);
  // TODO: this is actually one of three values:
  // CYPRESS_TOUCH_OPERATE_MODE, CYPRESS_TOUCH_LOW_POWER_MODE, CYPRESS_TOUCH_DEEP_SLEEP_MODE
  // -> add enum?
  return state == CYPRESS_TOUCH_OPERATE_MODE;
}

void CypressTouchscreen::hard_reset_() {
  // tp: done
  this->rts_pin_->digital_write(true);
  delay(10);
  this->rts_pin_->digital_write(false);
  delay(2);
  this->rts_pin_->digital_write(true);
  delay(10);
}

bool CypressTouchscreen::soft_reset_() {
  // tp: done
  uint8_t data[] = {CYPRESS_TOUCH_BASE_ADDR, CYPRESS_TOUCH_SOFT_RST_MODE};
  this->rts_pin_->digital_write(true);
  auto err = this->write(data, 2);
  if (err != i2c::ERROR_OK)
    return false;
  // Wait a little bit
  delay(20);
  return true;
}

void CypressTouchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "Cypress Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  RTS Pin: ", this->rts_pin_);
}

}  // namespace cypress_touch
}  // namespace esphome
