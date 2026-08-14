#include "ftp_poc_endpoint.h"

#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include "ftp_endpoint.h"
#include "nvmPartition.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include "device.h"
}


namespace {

constexpr uint32_t kOperationTimeoutMs = 5000;
constexpr char kEndpointName[] = "poc";
constexpr size_t kPocTextCapacity = 256;

NvmPartition *poc_partition;
uint32_t poc_length;

BmErr endpoint_read_at(void *context, uint32_t offset, uint8_t *buffer, uint16_t length) {
  NvmPartition *partition = static_cast<NvmPartition *>(context);
  if (!partition || offset > poc_length || length > poc_length - offset) {
    return BmEINVAL;
  }
  return partition->read(offset, buffer, length, kOperationTimeoutMs) ? BmOK : BmEIO;
}

BmErr endpoint_write_at(void *context, uint32_t offset, const uint8_t *data, uint16_t length) {
  NvmPartition *partition = static_cast<NvmPartition *>(context);
  if (!partition || offset > partition->size() - 1 ||
      length > partition->size() - 1 - offset) {
    return BmEINVAL;
  }
  return partition->write(offset, const_cast<uint8_t *>(data), length, kOperationTimeoutMs)
             ? BmOK
             : BmEIO;
}

BmErr endpoint_close(void *context) { return context ? BmOK : BmEINVAL; }

BmErr endpoint_finalize(void *context, uint32_t total_size, uint16_t crc16) {
  NvmPartition *partition = static_cast<NvmPartition *>(context);
  if (!partition || total_size == 0 || total_size >= partition->size()) {
    return BmEINVAL;
  }
  uint16_t computed_crc16;
  if (!partition->crc16(0, total_size, computed_crc16, kOperationTimeoutMs)) {
    return BmEIO;
  }
  if (computed_crc16 != crc16) {
    return BmEBADMSG;
  }
  poc_length = total_size;
  return BmOK;
}

BmErr endpoint_open_source(const uint8_t *spec, uint16_t spec_len, BmFtpSource *source) {
  if (!poc_partition || spec_len != strlen(kEndpointName) ||
      memcmp(spec, kEndpointName, spec_len) != 0) {
    return BmEINVAL;
  }
  uint16_t crc16;
  if (!poc_partition->crc16(0, poc_length, crc16, kOperationTimeoutMs)) {
    return BmEIO;
  }
  source->context = poc_partition;
  source->total_size = poc_length;
  source->crc16 = crc16;
  source->read_at = endpoint_read_at;
  source->close = endpoint_close;
  return BmOK;
}

BmErr endpoint_open_sink(const uint8_t *spec, uint16_t spec_len, uint32_t total_size,
                         BmFtpSink *sink) {
  if (!poc_partition || spec_len != strlen(kEndpointName) ||
      memcmp(spec, kEndpointName, spec_len) != 0 || total_size == 0 ||
      total_size >= poc_partition->size()) {
    return BmEINVAL;
  }
  sink->context = poc_partition;
  sink->write_at = endpoint_write_at;
  sink->finalize = endpoint_finalize;
  sink->abort = endpoint_close;
  sink->close = endpoint_close;
  return BmOK;
}

const BmFtpEndpointOps endpoint_ops = {
    .kind = BmFtpEndpointFlash,
    .open_source = endpoint_open_source,
    .open_sink = endpoint_open_sink,
};

BaseType_t ftp_poc_command(char *write_buffer, size_t write_buffer_len,
                           const char *command_string) {
  (void)write_buffer;
  (void)write_buffer_len;

  BaseType_t command_len;
  const char *command = FreeRTOS_CLIGetParameter(command_string, 1, &command_len);
  if (!command || strncmp("print", command, command_len) != 0) {
    printf("ERR Usage: ftp-poc print\n");
    return pdFALSE;
  }
  if (!poc_partition || poc_length == 0 || poc_length >= poc_partition->size()) {
    printf("ERR FTP POC data unavailable\n");
    return pdFALSE;
  }

  char *text = static_cast<char *>(pvPortMalloc(poc_length + 1));
  configASSERT(text);
  if (!poc_partition->read(0, reinterpret_cast<uint8_t *>(text), poc_length,
                           kOperationTimeoutMs)) {
    printf("ERR Failed to read FTP POC flash\n");
  } else {
    text[poc_length] = '\0';
    printf("%s\n", text);
  }
  vPortFree(text);
  return pdFALSE;
}

const CLI_Command_Definition_t ftp_poc_cli_command = {
    "ftp-poc",
    "ftp-poc:\n"
    " * ftp-poc print\n",
    ftp_poc_command,
    1,
};

} // namespace

void ftp_poc_flash_endpoint_init(NvmPartition *partition) {
  configASSERT(partition);
  poc_partition = partition;
  char poc_text[kPocTextCapacity];
  int poc_text_length = snprintf(
      poc_text, sizeof(poc_text),
      "Bristlemouth FTP proof of concept from mote %016" PRIx64 ".",
      node_id());
  if (poc_text_length < 0 || static_cast<size_t>(poc_text_length) >= sizeof(poc_text)) {
    printf("Failed to format FTP POC flash text\n");
    return;
  }
  poc_length = static_cast<uint32_t>(poc_text_length);

  if (!poc_partition->write(0, reinterpret_cast<uint8_t *>(poc_text), poc_length,
                            kOperationTimeoutMs)) {
    printf("Failed to seed FTP POC flash\n");
  }
  if (bm_ftp_endpoint_register(&endpoint_ops) != BmOK) {
    printf("Failed to register FTP POC flash endpoint\n");
  }
  FreeRTOS_CLIRegisterCommand(&ftp_poc_cli_command);
}
