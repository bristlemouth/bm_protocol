/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include <inttypes.h>
#include "debug.h"
#include "bm_network.h"
#include "bm_pubsub.h"
#include "lwip/inet.h"

static BaseType_t neighborsCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdGpio = {
  // Command string
  "bm",
  // Help string
  "bm:\n"
  " * bm neighbors - show neighbors + liveliness\n"
  " * bm topo - print network topology\n"
  " * bm info <ip addr> - Request fw info from device\n"
  " * bm list-topics - get pub/sub capabilities of other nodes on network\n"
  " * bm sub <topic>\n"
  " * bm unsub <topic>\n"
  " * bm pub <topic> <data>\n"
  " * bm print\n",
  // Command function
  neighborsCommand,
  // Number of parameters (variable)
  -1
};

static void print_subscriptions(char* topic, uint16_t topic_len, char* data, uint16_t data_len) {
    if (strncmp("hydrophone/db", topic, topic_len) == 0) {
        if(data_len == sizeof(float)) {
            float dbLevel;
            memcpy(&dbLevel, data, data_len);
            printf("RX %0.1f dB\n", dbLevel);
        }
    } else {
        printf("Received data on topic: %.*s\n", topic_len, topic);
        printf("Data: %.*s\n", data_len, data);
    }
}

void debugBMInit(void) {
  FreeRTOS_CLIRegisterCommand( &cmdGpio );
}

static BaseType_t neighborsCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {
    const char *parameter;
    BaseType_t parameterStringLength;

    // Remove unused argument warnings
    ( void ) commandString;
    ( void ) writeBuffer;
    ( void ) writeBufferLen;

    do {
        parameter = FreeRTOS_CLIGetParameter(
                    commandString,
                    1, // Get the first parameter (command)
                    &parameterStringLength);

        if(parameter == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }

        if (strncmp("neighbors", parameter,parameterStringLength) == 0) {
            bm_network_print_neighbor_table();
        } else if (strncmp("topo", parameter,parameterStringLength) == 0) {
            bm_network_request_neighbor_tables();
            vTaskDelay(10);
            bm_network_print_topology(NULL, NULL, 0);
            printf("\n");
        } else if (strncmp("info", parameter, parameterStringLength) == 0) {
            BaseType_t addrStrLen;
            const char *addrStr = FreeRTOS_CLIGetParameter(
            commandString,
            2, // Get the second parameter (address)
            &addrStrLen);

            if(addrStr == NULL) {
                printf("ERR Invalid parameters\n");
                break;
            }

            ip_addr_t addr;
            if(!inet6_aton(addrStr, &addr)) {
                printf("ERR Invalid address\n");
                break;
            }

            bm_network_request_fw_info(&addr);

        } else if (strncmp("list-topics", parameter,parameterStringLength) == 0) {
            bm_network_request_caps();

        } else if (strncmp("sub", parameter,parameterStringLength) == 0) {
            const char *topic = FreeRTOS_CLIGetParameter(
                            commandString,
                            2,
                            &parameterStringLength);

            if(parameterStringLength == 0) {
                printf("ERR topic required\n");
                break;
            }

            bm_sub_t subscription;
            subscription.topic_len = parameterStringLength;
            subscription.topic = pvPortMalloc(subscription.topic_len);
            configASSERT(subscription.topic);
            memcpy(subscription.topic, topic, subscription.topic_len);
            subscription.cb = print_subscriptions;

            bm_pubsub_subscribe(&subscription);
            vPortFree(subscription.topic);

        } else if (strncmp("unsub", parameter,parameterStringLength) == 0) {
            const char *topic = FreeRTOS_CLIGetParameter(
                            commandString,
                            2,
                            &parameterStringLength);

            if(parameterStringLength == 0) {
                printf("ERR topic required\n");
                break;
            }

            bm_sub_t subscription;
            subscription.topic_len = parameterStringLength;
            subscription.topic = pvPortMalloc(subscription.topic_len);
            configASSERT(subscription.topic);
            memcpy(subscription.topic, topic, subscription.topic_len);
            subscription.cb = NULL;

            bm_pubsub_unsubscribe(&subscription);
            vPortFree(subscription.topic);

        } else if (strncmp("pub", parameter,parameterStringLength) == 0) {
            const char *topic = FreeRTOS_CLIGetParameter(
                            commandString,
                            2,
                            &parameterStringLength);

            if(parameterStringLength == 0) {
                printf("ERR topic required\n");
                break;
            }

            bm_pub_t publication;
            publication.topic_len = parameterStringLength;
            publication.topic = pvPortMalloc(publication.topic_len);
            configASSERT(publication.topic);
            memcpy(publication.topic, topic, publication.topic_len);

            const char *data = FreeRTOS_CLIGetParameter(
                            commandString,
                            3,
                            &parameterStringLength);

            if(parameterStringLength == 0) {
                printf("ERR data required\n");
                break;
            }

            publication.data_len = parameterStringLength;
            publication.data = pvPortMalloc(publication.data_len);
            configASSERT(publication.data);
            memcpy(publication.data, data, publication.data_len);

            bm_pubsub_publish(&publication);
            vPortFree(publication.topic);
            vPortFree(publication.data);
        } else if (strncmp("print", parameter,parameterStringLength) == 0) {
            bm_pubsub_print_subs();
        } else {
            printf("ERR Invalid paramters\n");
            break;
        }
    } while(0);

    return pdFALSE;
}
