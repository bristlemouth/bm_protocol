#ifndef _AUTOFAKE_FREERTOS_H
#define _AUTOFAKE_FREERTOS_H

#include "fff.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "timers.h"
#include "task.h"

DECLARE_FAKE_VALUE_FUNC(EventBits_t, xEventGroupWaitBits, EventGroupHandle_t,const EventBits_t,const BaseType_t,const BaseType_t,TickType_t);
DECLARE_FAKE_VALUE_FUNC(EventBits_t, xEventGroupClearBits, EventGroupHandle_t, const EventBits_t);
DECLARE_FAKE_VALUE_FUNC(EventGroupHandle_t, xEventGroupCreateStatic, StaticEventGroup_t * );
DECLARE_FAKE_VALUE_FUNC(EventBits_t,xEventGroupSetBits, EventGroupHandle_t, const EventBits_t);
DECLARE_FAKE_VALUE_FUNC(QueueHandle_t,xQueueGenericCreateStatic, const UBaseType_t, const UBaseType_t, uint8_t *,StaticQueue_t *, const uint8_t );
DECLARE_FAKE_VALUE_FUNC(BaseType_t, xQueueGenericSend,QueueHandle_t, const void * const, TickType_t, const BaseType_t);
DECLARE_FAKE_VALUE_FUNC(BaseType_t, xQueueReceive, QueueHandle_t, void * const, TickType_t);
DECLARE_FAKE_VALUE_FUNC(QueueHandle_t,xQueueGenericCreate, const UBaseType_t ,const UBaseType_t , const uint8_t );
DECLARE_FAKE_VALUE_FUNC(TimerHandle_t, xTimerCreate, const char * const, const TickType_t,const BaseType_t , void * const ,TimerCallbackFunction_t);
DECLARE_FAKE_VALUE_FUNC(BaseType_t, xTaskCreate, TaskFunction_t ,const char * const , const configSTACK_DEPTH_TYPE ,void * const ,UBaseType_t ,TaskHandle_t * const  );
DECLARE_FAKE_VALUE_FUNC(BaseType_t, xTimerGenericCommand, TimerHandle_t , const BaseType_t ,const TickType_t , BaseType_t * const ,const TickType_t  );
DECLARE_FAKE_VALUE_FUNC(EventGroupHandle_t, xEventGroupCreate);
DECLARE_FAKE_VALUE_FUNC(BaseType_t, xTaskGenericNotify, TaskHandle_t,UBaseType_t, uint32_t, eNotifyAction , uint32_t * );
DECLARE_FAKE_VALUE_FUNC(BaseType_t, xTaskGenericNotifyWait, UBaseType_t ,uint32_t ,uint32_t , uint32_t * , TickType_t  );

#endif  // _AUTOFAKE_FREERTOS_H
