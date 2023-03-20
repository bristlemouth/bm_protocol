#include "debug_w25.h"
#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "cli.h"
#include <stdlib.h>
#include "crc.h"

#define BITMASK24BIT (0x00ffffff)
#define MAX_READ_LEN_BYTES (2048)
#define MAX_WRITE_LEN_BYTES (MAX_READ_LEN_BYTES)
#define BIG_WRITE_BLOCK_LEN (MAX_WRITE_LEN_BYTES)

static W25* _w25_instance = NULL;

static BaseType_t w25Command( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdW25 = {
  // Command string
  "w25",
  // Help string
  "w25:\n"
  " * w25 r <addr> <len>\n"
  " * w25 w <addr> <str>\n"
  " * w25 es <addr>\n"
  " * w25 ec\n"
  " * w25 bw <addr> <len>\n"
  " * w25 test <addr> <len>\n",
  // Command function
  w25Command,
  // Number of parameters (variable)
  -1
};

void debugW25Init(W25* w25) {
    _w25_instance = w25;
    FreeRTOS_CLIRegisterCommand( &cmdW25 );
}

static BaseType_t w25Command( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {
    BaseType_t parameterStringLength;

    ( void ) writeBuffer;
    ( void ) writeBufferLen;

    do {
        if(!_w25_instance) {
            printf("w25 not initialized.\n");
            break;
        }
        const char *parameter = FreeRTOS_CLIGetParameter(
                commandString,
                1, // Get the first parameter (command)
                &parameterStringLength);

        if(parameter == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }
        if (strncmp("r", parameter, parameterStringLength) == 0) {
            const char *addr = FreeRTOS_CLIGetParameter(
                            commandString,
                            2,
                            &parameterStringLength);
            if(parameterStringLength == 0) {
                printf("ERR Invalid read paramters\n");
                break;
            }
            const char *len = FreeRTOS_CLIGetParameter(
                            commandString,
                            3,
                            &parameterStringLength);
            if(parameterStringLength == 0) {
                printf("ERR Invalid read paramters\n");
                break;
            }
            uint32_t address = atoi(addr) & BITMASK24BIT;
            uint32_t read_len = atoi(len);
            if(read_len > MAX_READ_LEN_BYTES) {
                printf("Read size too large\n");
                break;
            }
            uint8_t* read_buf = (uint8_t *)pvPortMalloc(read_len);
            configASSERT(read_buf);
            if(!_w25_instance->read(address, read_buf, read_len)){
                printf("Read failed\n");
            }
            printf("Byte string:\n");
            for(uint32_t i  = 0; i < read_len; i++){
                if(i>0){
                    printf(":");
                }
                if(i%8==0){
                    printf("\n");
                }
                printf("%02X", read_buf[i]);
            }
            printf("\n");
            vPortFree(read_buf);
        } else if (strncmp("w", parameter, parameterStringLength) == 0) {
            const char *addr = FreeRTOS_CLIGetParameter(
                            commandString,
                            2,
                            &parameterStringLength);
            if(parameterStringLength == 0) {
                printf("ERR Invalid write paramters\n");
                break;
            }
            const char *str = FreeRTOS_CLIGetParameter(
                            commandString,
                            3,
                            &parameterStringLength);
            if(parameterStringLength == 0) {
                printf("ERR Invalid write paramters\n");
                break;
            }
            uint32_t address = atoi(addr) & BITMASK24BIT;
            size_t write_len = parameterStringLength;
            if(write_len > MAX_WRITE_LEN_BYTES) {
                printf("Write size too large\n");
                break;
            }
            uint8_t* wr_buf = (uint8_t *)pvPortMalloc(write_len);
            configASSERT(wr_buf);
            memcpy(wr_buf, str, write_len);
            if(!_w25_instance->write(address, wr_buf, write_len)){
                printf("Write failed\n");
            } else {
                printf("Write succeded!\n");
            }
            vPortFree(wr_buf);
        } else if (strncmp("es", parameter, parameterStringLength) == 0) {
            const char *addr = FreeRTOS_CLIGetParameter(
                            commandString,
                            2,
                            &parameterStringLength);
            if(parameterStringLength == 0) {
                printf("ERR Invalid erase sector paramters\n");
                break;
            }
            uint32_t address = atoi(addr) & BITMASK24BIT;
            if(!_w25_instance->eraseSector(address)){
                printf("Erase sector failed\n");
            } else {
                printf("Erase sector succeded!\n");
            }
        }  else if (strncmp("ec", parameter, parameterStringLength) == 0) {
            if(!_w25_instance->eraseChip()){
                printf("Erase chip failed\n");
            } else {
                printf("Erase chip succeded!\n");
            }
        } else if (strncmp("bw", parameter, parameterStringLength) == 0) {
            const char *addr = FreeRTOS_CLIGetParameter(
                            commandString,
                            2,
                            &parameterStringLength);
            if(parameterStringLength == 0) {
                printf("ERR Invalid write paramters\n");
                break;
            }
            const char *len = FreeRTOS_CLIGetParameter(
                            commandString,
                            3,
                            &parameterStringLength);
            if(parameterStringLength == 0) {
                printf("ERR Invalid write paramters\n");
                break;
            }
            uint32_t address = atoi(addr) & BITMASK24BIT;
            uint32_t write_len = atoi(len);
            uint32_t blocklen = (write_len < BIG_WRITE_BLOCK_LEN) ? write_len : BIG_WRITE_BLOCK_LEN;
            uint8_t* wr_buf = (uint8_t *)pvPortMalloc(blocklen);
            configASSERT(wr_buf);
            for(uint32_t i = 0; i < blocklen; i++) {
                wr_buf[i] = 0xAA + ((i % 5) * 0x11);
            }
            uint32_t offset = 0;
            while(write_len){
                printf("Writing addr %lu, size %lu \n", (address + offset), blocklen);
                if(!_w25_instance->write(address + offset, wr_buf, blocklen)){
                    printf("Write failed\n");
                    break;
                } 
                offset += blocklen;
                write_len -= blocklen;
                blocklen = (write_len < BIG_WRITE_BLOCK_LEN) ? write_len : BIG_WRITE_BLOCK_LEN;
            }
            if(!write_len){
                printf("Write succeded!\n");
            }
            vPortFree(wr_buf);
        }  else if (strncmp("test", parameter, parameterStringLength) == 0) {
            const char *addr = FreeRTOS_CLIGetParameter(
                            commandString,
                            2,
                            &parameterStringLength);
            if(parameterStringLength == 0) {
                printf("ERR Invalid write paramters\n");
                break;
            }
            const char *len = FreeRTOS_CLIGetParameter(
                            commandString,
                            3,
                            &parameterStringLength);
            if(parameterStringLength == 0) {
                printf("ERR Invalid write paramters\n");
                break;
            }
            uint32_t address = atoi(addr) & BITMASK24BIT;
            uint32_t write_len = atoi(len);
            uint32_t blocklen = (write_len < BIG_WRITE_BLOCK_LEN) ? write_len : BIG_WRITE_BLOCK_LEN;
            uint8_t* wr_buf = (uint8_t *)pvPortMalloc(blocklen);
            uint32_t crc32_write = 0;
            uint32_t crc32_read = 0;
            bool first_loop = true;
            configASSERT(wr_buf);
            for(uint32_t i = 0; i < blocklen; i++) {
                wr_buf[i] = 0xAA + ((i % 5) * 0x11);
            }
            uint32_t offset = 0;
            do {
                while(write_len){
                    printf("Writing addr %lu, size %lu \n", (address + offset), blocklen);
                    if(!_w25_instance->write(address + offset, wr_buf, blocklen)){
                        printf("Write failed\n");
                        break;
                    } 
                    if(first_loop) {
                        crc32_write = crc32_ieee(wr_buf, blocklen);
                        first_loop = false;
                    } else {
                        crc32_write = crc32_ieee_update(crc32_write, wr_buf, blocklen);
                    }
                    offset += blocklen;
                    write_len -= blocklen;
                    blocklen = (write_len < BIG_WRITE_BLOCK_LEN) ? write_len : BIG_WRITE_BLOCK_LEN;
                }
                if(!write_len){
                    printf("Write succeded!\n");
                }  else {
                    printf("Write failed!\n");
                    break;
                }
                if(!_w25_instance->crc32Checksum(address,atoi(len),crc32_read)){
                    printf("Read CRC32 failed!\n");
                    break;
                }
            } while(0);
            if(crc32_write == crc32_read){
                printf("flash test success, crc_w:%lu, crc_r:%lu\n", crc32_write, crc32_read);
            } else {
                printf("flash test failed, crc_w:%lu, crc_r:%lu\n", crc32_write, crc32_read);
            }
            vPortFree(wr_buf);
        } else {
            printf("ERR Invalid paramters\n");
            break;
        }
    } while(0);
    return pdFALSE;
} 