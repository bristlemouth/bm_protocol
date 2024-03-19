#include "device_test_service.h"
#include "FreeRTOS.h"
#include "bm_service.h"
#include "bm_service_common.h"
#include "device_info.h"
#include "device_test_svc_reply_msg.h"
#include "device_test_svc_request_msg.h"

#define DEVICE_TEST_SERVICE_SUFFIX "/device-test"

static device_test_f _device_test_func;

static bool device_test_service_handler(size_t service_strlen, const char *service,
                                        size_t req_data_len, uint8_t *req_data,
                                        size_t &buffer_len, uint8_t *reply_data);

void device_test_service_init(device_test_f f) {
  configASSERT(f);
  _device_test_func = f;
  static char device_test_service_str[BM_SERVICE_MAX_SERVICE_STRLEN];
  size_t topic_strlen = snprintf(device_test_service_str, sizeof(device_test_service_str),
                                 "%" PRIx64 "%s", getNodeId(), DEVICE_TEST_SERVICE_SUFFIX);
  if (topic_strlen > 0) {
    bm_service_register(topic_strlen, device_test_service_str, device_test_service_handler);
  } else {
    printf("Failed to register device test service\n");
  }
}

bool device_test_service_request(uint64_t target_node_id, void *data, size_t len,
                                 bm_service_reply_cb reply_cb, uint32_t timeout_s) {
  bool rval = false;
  char *target_service_str = static_cast<char *>(pvPortMalloc(BM_SERVICE_MAX_SERVICE_STRLEN));
  configASSERT(target_service_str);
  size_t target_service_strlen =
      snprintf(target_service_str, BM_SERVICE_MAX_SERVICE_STRLEN, "%" PRIx64 "%s",
               target_node_id, DEVICE_TEST_SERVICE_SUFFIX);
  DeviceTestSvcRequestMsg::Data req = {len, static_cast<uint8_t *>(data)};
  size_t cbor_buflen =
      sizeof(DeviceTestSvcRequestMsg::Data) + len + 25; // Abitrary cbor overhead padding.
  uint8_t *cbor_buffer = static_cast<uint8_t *>(pvPortMalloc(cbor_buflen));
  configASSERT(cbor_buffer);
  size_t req_data_len;
  do {
    if (DeviceTestSvcRequestMsg::encode(req, cbor_buffer, cbor_buflen, &req_data_len) !=
        CborNoError) {
      printf("Failed to encode config map request\n");
      break;
    }
    if (target_service_strlen > 0) {
      rval = bm_service_request(target_service_strlen, target_service_str, req_data_len,
                                cbor_buffer, reply_cb, timeout_s);
    }
  } while (0);
  vPortFree(cbor_buffer);
  vPortFree(target_service_str);
  return rval;
}

static bool device_test_service_handler(size_t service_strlen, const char *service,
                                        size_t req_data_len, uint8_t *req_data,
                                        size_t &buffer_len, uint8_t *reply_data) {
  configASSERT(_device_test_func);
  bool rval = false;
  printf("Data received on service: %.*s\n", service_strlen, service);
  do {
    DeviceTestSvcRequestMsg::Data req;
    if (DeviceTestSvcRequestMsg::decode(req, req_data, req_data_len) != CborNoError) {
      printf("Failed to decode device test service request\n");
      break;
    }
    DeviceTestSvcReplyMsg::Data rep = {false, 0, NULL};
    do {
      rep.data = _device_test_func(rep.success, req.data, req.data_len, rep.data_len);
      if (rep.data == NULL) {
        configASSERT(rep.data_len == 0);
      }
      size_t encoded_len;
      // Will return CborErrorOutOfMemory if buffer_len is too small
      if (DeviceTestSvcReplyMsg::encode(rep, reply_data, buffer_len, &encoded_len) !=
          CborNoError) {
        printf("Failed to encode sys info service reply\n");
        break;
      }
      buffer_len = encoded_len; // Pass back the encoded length
      rval = true;
    } while (0);
    if (rep.data) {
      vPortFree(rep.data);
    }
  } while (0);

  return rval;
}