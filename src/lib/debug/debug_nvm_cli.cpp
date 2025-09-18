#include "debug_nvm_cli.h"
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"
#include "app_util.h"
#include <stdio.h>
#include <stdlib.h>
#include "mbedtls_base64/base64.h"
#define NVM_WR_R_TIMEOUT_MS (10 * 1000)

NvmPartition* _debug_cli_nvm_partition = NULL;
NvmPartition* _dfu_cli_nvm_partition = NULL;

static BaseType_t nvmCliCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);
static bool b64WriteToNvm(NvmPartition *partition, uint32_t addr, const unsigned char * b64DataStr, size_t b64DataLen);
static bool b64ReadFromNvm(const unsigned char * buf, size_t binLen);

static const CLI_Command_Definition_t cmdNvmCli = {
  // Command string
  "nvm",
  // Help string
  "nvm:\n"
  " * nvm b64write <debug/dfu> <addr> <b64 data>\n"
  " * nvm b64read <debug/dfu> <addr> <bin_len>\n"
  " * nvm crc16 <debug/dfu> <addr> <bin_len>\n"
  " * nvm erase <debug/dfu> <addr> <bin_len>\n",
  // Command function
  nvmCliCommand,
  // Number of parameters (variable)
  -1
};

void debugNvmCliInit(NvmPartition *debug_cli_nvm_partition,  NvmPartition *dfu_cli_partition) {
    configASSERT(debug_cli_nvm_partition);
    configASSERT(dfu_cli_partition);
    _debug_cli_nvm_partition = debug_cli_nvm_partition;
    _dfu_cli_nvm_partition = dfu_cli_partition;
    FreeRTOS_CLIRegisterCommand( &cmdNvmCli );
}

static BaseType_t nvmCliCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {
    // Remove unused argument warnings
    ( void ) commandString;
    ( void ) writeBuffer;
    ( void ) writeBufferLen;

    do {
        BaseType_t opStrLen;
        const char *opStr = FreeRTOS_CLIGetParameter(
        commandString,
        1,
        &opStrLen);
        if(opStr == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }

        BaseType_t partitionStrLen;
        const char *partitionStr = FreeRTOS_CLIGetParameter(
        commandString,
        2,
        &partitionStrLen);
        if(partitionStr == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }
        NvmPartition* partition;
        if (strncmp("debug", partitionStr,partitionStrLen) == 0) {
            partition = _debug_cli_nvm_partition;
        } else if(strncmp("dfu", partitionStr,partitionStrLen) == 0){
            partition = _dfu_cli_nvm_partition;
        } else {
            printf("ERR Invalid paramters\n");
            break;
        }

        BaseType_t addrStrLen;
        const char *addrStr = FreeRTOS_CLIGetParameter(
        commandString,
        3,
        &addrStrLen);
        if(addrStr == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }
        uint32_t addr = strtoul(addrStr, NULL, 0);
        if (strncmp("b64write", opStr,opStrLen) == 0) {
            BaseType_t b64DataLen;
            const char *b64DataStr = FreeRTOS_CLIGetParameter(
            commandString,
            4,
            &b64DataLen);
            if(b64DataStr == NULL) {
                printf("ERR Invalid paramters\n");
                break;
            }
            if (addr + b64DataLen > partition->size()){
                printf("ERR Invalid addr\n");
                break;
            }
            if(!b64WriteToNvm(partition, (uint32_t)addr,(const unsigned char *)b64DataStr, b64DataLen)){
                printf("Failed to write\n");
            } else {
                printf("Write success!#\n");
            }
        } else if(strncmp("b64read", opStr,opStrLen) == 0){
            BaseType_t lenStrLen;
            const char *lenStr = FreeRTOS_CLIGetParameter(
            commandString,
            4,
            &lenStrLen);
            if(lenStr == NULL) {
                printf("ERR Invalid paramters\n");
                break;
            }
            uint32_t len = strtoul(lenStr, NULL, 0);
            if (addr + len > partition->size()){
                printf("ERR Invalid addr\n");
                break;
            }
            char* buf = (char*)pvPortMalloc(len+1);
            configASSERT(buf);
            buf[len] = '\0';
            if(!partition->read(addr,(uint8_t*)buf,len,NVM_WR_R_TIMEOUT_MS)){
                printf("Failed to read nvm\n");
            } else {
                if(!b64ReadFromNvm((const unsigned char *)buf, len)){
                    printf("Failed to read\n");
                } else {
                    printf("Read success!#\n");
                }
            }
            vPortFree(buf);
        } else if(strncmp("crc16", opStr,opStrLen) == 0){
            BaseType_t lenStrLen;
            const char *lenStr = FreeRTOS_CLIGetParameter(
            commandString,
            4,
            &lenStrLen);
            if(lenStr == NULL) {
                printf("ERR Invalid paramters\n");
                break;
            }
            uint32_t len = strtoul(lenStr, NULL, 0);;
            if (addr + len > partition->size()){
                printf("ERR Invalid addr\n");
                break;
            }
            uint16_t crc;
            if(!partition->crc16(addr,len,crc,NVM_WR_R_TIMEOUT_MS)){
                printf("Failed to get crc16\n");
            } else {
                printf("<crc16>0x%x#\n", crc);
            }
        } else if(strncmp("erase", opStr,opStrLen) == 0){
            BaseType_t lenStrLen;
            const char *lenStr = FreeRTOS_CLIGetParameter(
            commandString,
            4,
            &lenStrLen);
            if(lenStr == NULL) {
                printf("ERR Invalid paramters\n");
                break;
            }
            uint32_t len = strtoul(lenStr, NULL, 0);;
            if (addr + len > partition->size()){
                printf("ERR Invalid addr\n");
                break;
            }
            if((uint32_t)addr % partition->alignment() != 0){
                printf("ERR Invalid addr, must be %lu aligned\n", partition->alignment());
                break;
            }
            if(!partition->erase(addr,len, NVM_WR_R_TIMEOUT_MS)){
                printf("Failed to erase nvm\n");
            } else {
                printf("Succesfully erased %lu bytes at addr %lx!#\n",len, addr);
            }
        } else {
            printf("ERR Invalid paramters\n");
            break;
        }
    } while(0);

    return pdFALSE;
}

static bool b64WriteToNvm(NvmPartition* partition, uint32_t addr, const unsigned char * b64DataStr, size_t b64DataLen) {
    bool rval = false;
    size_t bin_len;
    uint8_t * buf = NULL;
    do {
        mbedtls_base64_decode(NULL, 0, &bin_len, b64DataStr,b64DataLen);
        buf = (uint8_t *)pvPortMalloc(bin_len);
        configASSERT(buf);
        if(mbedtls_base64_decode(buf, bin_len, &bin_len, b64DataStr,b64DataLen) != 0){
            printf("Failed to decode b64 str\n");
            break;
        }
        if(!partition->write(addr, buf,bin_len,NVM_WR_R_TIMEOUT_MS)){
            printf("Failed to write NVM\n");
            break;
        } else {
            printf("Succesfully wrote to addr %lx\n", addr);
        }
        rval = true;
    } while(0);
    vPortFree(buf);
    return rval;
}

static bool b64ReadFromNvm(const unsigned char * buf, size_t binLen) {
    bool rval = false;
    size_t b64Len;
    unsigned char * b64buf = NULL;
    do {
        mbedtls_base64_encode(NULL, 0, &b64Len, buf ,binLen);
        b64buf = (unsigned char *)pvPortMalloc(b64Len);
        configASSERT(b64buf);
        if(mbedtls_base64_encode(b64buf, b64Len, &b64Len, buf, binLen) != 0){
            printf("Failed to decode b64 str\n");
            break;
        } else {
            printf("\n%s\n",b64buf);
        }
        rval = true;
    } while(0);
    vPortFree(b64buf);
    return rval;
}
