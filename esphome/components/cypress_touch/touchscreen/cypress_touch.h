#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace cypress_touch {

using namespace touchscreen;

class CypressTouchscreen : public Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_rts_pin(GPIOPin *pin) { this->rts_pin_ = pin; }

  void set_power_state(bool enable);
  bool get_power_state();

 protected:
  void hard_reset_();
  bool soft_reset_();
  void update_touches() override;

  InternalGPIOPin *interrupt_pin_;
  GPIOPin *rts_pin_;

 private:
  bool ping_touchscreen(int retries);
  bool write_registers(uint8_t cmd, uint8_t *buffer, int len);
  bool read_registers(uint8_t cmd, uint8_t *buffer, int len);
  bool load_bootloader_registers(struct bootloader_data *blData);
  bool exit_bootloader_mode();
  bool set_sysinfo_mode(struct sysinfo_data *data);
  bool set_sysinfo_registers(sysinfo_data *data);
  void handshake();
  bool get_touch_data(touch_data *report);


  typedef struct bootloader_data {
    uint8_t bl_file;
    uint8_t bl_status;
    uint8_t bl_error;
    uint8_t blver_hi;
    uint8_t blver_lo;
    uint8_t bld_blver_hi;
    uint8_t bld_blver_lo;
    uint8_t ttspver_hi;
    uint8_t ttspver_lo;
    uint8_t appid_hi;
    uint8_t appid_lo;
    uint8_t appver_hi;
    uint8_t appver_lo;
    uint8_t cid_0;
    uint8_t cid_1;
    uint8_t cid_2;
  } bootloader_data_t;

  struct sysinfo_data {
    uint8_t hst_mode;
    uint8_t mfg_stat;
    uint8_t mfg_cmd;
    uint8_t cid[3];
    uint8_t tt_undef1;
    uint8_t uid[8];
    uint8_t bl_verh;
    uint8_t bl_verl;
    uint8_t tts_verh;
    uint8_t tts_verl;
    uint8_t app_idh;
    uint8_t app_idl;
    uint8_t app_verh;
    uint8_t app_verl;
    uint8_t tt_undef[5];
    uint8_t scn_typ;
    uint8_t act_intrvl;
    uint8_t tch_tmout;
    uint8_t lp_intrvl;
  };

  struct touch_data {
    uint8_t fingers;
    uint16_t x[2];
    uint16_t y[2];
    uint8_t z[2];
    uint8_t detectionType;
  };

  bootloader_data_t bootloader_data_;
};

}  // namespace cypress_touch
}  // namespace esphome
