#include "mock_FreeRTOS.h"

DEFINE_FAKE_VALUE_FUNC(EventBits_t, xEventGroupWaitBits, EventGroupHandle_t,const EventBits_t,const BaseType_t,const BaseType_t,TickType_t);
DEFINE_FAKE_VALUE_FUNC(EventBits_t, xEventGroupClearBits, EventGroupHandle_t, const EventBits_t);
DEFINE_FAKE_VALUE_FUNC(EventGroupHandle_t, xEventGroupCreateStatic, StaticEventGroup_t * );
DEFINE_FAKE_VALUE_FUNC(EventBits_t,xEventGroupSetBits, EventGroupHandle_t, const EventBits_t);
DEFINE_FAKE_VALUE_FUNC(QueueHandle_t,xQueueGenericCreateStatic, const UBaseType_t, const UBaseType_t, uint8_t *,StaticQueue_t *, const uint8_t );
DEFINE_FAKE_VALUE_FUNC(BaseType_t, xQueueGenericSend,QueueHandle_t, const void * const, TickType_t, const BaseType_t);
DEFINE_FAKE_VALUE_FUNC(BaseType_t, xQueueReceive, QueueHandle_t, void * const, TickType_t);
DEFINE_FAKE_VALUE_FUNC(QueueHandle_t,xQueueGenericCreate, const UBaseType_t ,const UBaseType_t , const uint8_t );
DEFINE_FAKE_VALUE_FUNC(TimerHandle_t, xTimerCreate, const char * const, const TickType_t,const BaseType_t , void * const ,TimerCallbackFunction_t  );
DEFINE_FAKE_VALUE_FUNC(BaseType_t, xTaskCreate, TaskFunction_t ,const char * const , const configSTACK_DEPTH_TYPE ,void * const ,UBaseType_t ,TaskHandle_t * const  );
DEFINE_FAKE_VALUE_FUNC(BaseType_t, xTimerGenericCommand, TimerHandle_t , const BaseType_t ,const TickType_t , BaseType_t * const ,const TickType_t  );
