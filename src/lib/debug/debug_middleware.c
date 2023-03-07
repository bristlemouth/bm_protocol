/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include "debug_middleware.h"
#include "bm_pubsub.h"

#include <string.h>

static BaseType_t middlewareCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdGpio = {
  // Command string
  "mw",
  // Help string
  "mw:\n"
  " * mw sub <topic>\n"
  " * mw unsub <topic>\n"
  " * mw pub <topic> <data>\n"
  " * mw print",
  // Command function
  middlewareCommand,
  // Number of parameters (variable)
  -1
};

static void print_subscriptions(char* topic, uint16_t topic_len, char* data, uint16_t data_len) {
    printf("Received data on topic: %.*s\n", topic_len, topic);
    printf("Data: %.*s\n", data_len, data);
}

void debugMiddlewareInit(void) {
  FreeRTOS_CLIRegisterCommand( &cmdGpio );
}

#define MESSAGE_SIZE (32)
static BaseType_t middlewareCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {
    BaseType_t parameterStringLength;

    // Remove unused argument warnings
    ( void ) commandString;
    ( void ) writeBuffer;
    ( void ) writeBufferLen;

    do {
        const char *parameter = FreeRTOS_CLIGetParameter(
                    commandString,
                    1, // Get the first parameter (command)
                    &parameterStringLength);

        if(parameter == NULL) {
            printf("ERR Invalid paramters\n");
            break;
        }

        if (strncmp("sub", parameter,parameterStringLength) == 0) {
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
        }
    } while(0);
    return pdFALSE;
}
