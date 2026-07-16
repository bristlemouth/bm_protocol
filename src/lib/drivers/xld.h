#ifndef __XLD_H__
#define __XLD_H__

#include "abstract_st_sensor.h"
#include "bm_config.h"
#include "bm_os.h"
#include "util.h"
#include <stdint.h>

class XLD {
public:
  XLD(SensorInterfaceBus *bus, void *arg);
  XLD(SensorInterfaceBus *bus, void *arg, uint8_t address);

  BmErr init(void);

  void handle_interrupt(void);

  BmErr request_reading(void);
  BmErr get_reading(float *mbar, float *temp, uint32_t timeout_ms = 500);

private:
  static constexpr uint8_t INIT_ADDRESS = 0x40;
  float m_pmin;
  float m_pmax;

  uint8_t m_address = INIT_ADDRESS;

  BmSemaphore m_data_ready_sem = nullptr;

  SensorInterfaceBus *m_bus = nullptr;
  void *m_arg = nullptr;

  typedef enum : uint8_t {
    CUST_ID0 = 0x00,
    CUST_ID1 = 0x01,
    GET_ADDR = 0x02,
    SCALING0 = 0x12,
    SCALING1 = 0x13,
    SCALING2 = 0x14,
    SCALING3 = 0x15,
    SCALING4 = 0x16,
    SET_ADDR = 0x42,
    CMD_MODE = 0xA9,
    GET_MEASURMENT = 0xAC,
  } XLDCmd;

  static constexpr uint8_t MODE_NORMAL = 0x00;
  static constexpr uint8_t MODE_CMD = 0x01;
  typedef struct {
    uint8_t : 2;
    uint8_t memory_err : 1;
    uint8_t mode : 2;
    uint8_t busy : 1;
    uint8_t : 2;
  } XLDStatus;

  static inline uint32_t convert_u32(uint16_t upper, uint16_t lower) {
    return (static_cast<uint32_t>(upper) << 16) | lower;
  }

  BmErr get_prod_id(uint32_t *id);
  BmErr calc_lim(XLDCmd initial, float *lim);
  BmErr read_memory_map(XLDCmd cmd, uint16_t *data);
  BmErr send_command(XLDCmd cmd);
  BmErr send_command(XLDCmd cmd, uint8_t *data, uint8_t size);
  BmErr read_device(XLDStatus *status, uint8_t *data, uint8_t size);
};

#endif
