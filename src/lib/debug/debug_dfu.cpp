#include "debug_dfu.h"
#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include "external_flash_partitions.h"
#include <stdio.h>
#include "dfu.h"

static BaseType_t dfuCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdDfu = {
  // Command string
  "dfu",
  // Help string
  "dfu (for binary loaded in flash):\n"
  " * dfu start <node id> <filter_key> <TimeoutMs>\n",
  // Command function
  dfuCommand,
  // Number of parameters (variable)
  -1
};

NvmPartition *_dfu_cli_partition;

void debugDfuInit(NvmPartition *dfu_cli_partition) {
    configASSERT(dfu_cli_partition);
    _dfu_cli_partition = dfu_cli_partition;
    FreeRTOS_CLIRegisterCommand( &cmdDfu );
}

void updateSuccessCallback(bool success, BmDfuErr err, uint64_t node_id) {
    if(success){
        printf("update successful %016" PRIx64 "\n", node_id);
    } else {
        printf("update failed, err: %d\n", err);
    }
}

static BaseType_t dfuCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {
    BaseType_t parameterStringLength;

    ( void ) writeBuffer;
    ( void ) writeBufferLen;

    do {
        const char *parameter = FreeRTOS_CLIGetParameter(
                commandString,
                1,
                &parameterStringLength);

        if(parameter == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }

        if (strncmp("start", parameter, parameterStringLength) == 0) {
            const char *nodeIdStr = FreeRTOS_CLIGetParameter(
                    commandString,
                    2,
                    &parameterStringLength);

            if(nodeIdStr == NULL) {
                printf("ERR Invalid paramters\n");
                break;
            }
            const char *filterKeyStr = FreeRTOS_CLIGetParameter(
                    commandString,
                    3,
                    &parameterStringLength);

            if(filterKeyStr == NULL) {
                printf("ERR Invalid paramters\n");
                break;
            }
            const char *timeoutMsStr = FreeRTOS_CLIGetParameter(
                    commandString,
                    4,
                    &parameterStringLength);

            if(timeoutMsStr == NULL) {
                printf("ERR Invalid paramters\n");
                break;
            }
            uint64_t node_id = strtoull(nodeIdStr, NULL, 0);
            uint32_t filter_key = strtoul(filterKeyStr, NULL, 0);
            uint32_t timeoutMS = strtoul(timeoutMsStr, NULL, 0);
            BmDfuImgInfo image_info;
            if(!_dfu_cli_partition->read(DFU_HEADER_OFFSET_BYTES, reinterpret_cast<uint8_t*>(&image_info), sizeof(BmDfuImgInfo), 1000)){
                printf("Failed to read DFU header.\n");
                break;
            }
            uint16_t crc;
            if(!_dfu_cli_partition->crc16(DFU_IMG_START_OFFSET_BYTES, image_info.image_size, crc, 10000)){
                printf("Failed to compute crc.\n");
                break;
            }
            if(crc != image_info.crc16){
                printf("CRCs don't match %x, %x, invalid image!\n", crc, image_info.crc16);
                break;
            }
            image_info.filter_key = filter_key;
            printf("Image valid, attempting to update\n");
            if(!bm_dfu_initiate_update(image_info, node_id, updateSuccessCallback, timeoutMS, true)){
                printf("Failed to start update\n");
            }

        } else {
            printf("ERR Invalid paramters\n");
        }

    } while(0);
    return pdFALSE;
}
