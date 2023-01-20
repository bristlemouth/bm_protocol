
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "adc.h"
#include "rtc.h"
#include "icache.h"
#include "ucpd.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

// Includes for FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

#include "serial.h"
#include "serial_console.h"
#include "cli.h"
#include "debug_memfault.h"
#include "debug_sys.h"
#include "gpioISR.h"
#include "bsp.h"
#include "bm_l2.h"
#include "printf.h"
#include "memfault_platform_core.h"

#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "lwip/inet.h"
#include "lwip/mld6.h"

#include <stdio.h>

// Get IPv6 LSB from CMAKE ALT_DEV define (use 0x01, 0x02, etc...)
#define IPV6_ADDR_LSB ALT_DEV

static void defaultTask(void *parameters);

static struct udp_pcb   *udp_pcb;
static uint16_t         udp_port;
struct netif            netif;

SerialHandle_t usart1 = {
    .device = USART1,
    .name = "usart1",
    .txPin = NULL,
    .rxPin = NULL,
    .txStreamBuffer = NULL,
    .rxStreamBuffer = NULL,
    .txBufferSize = 1024,
    .rxBufferSize = 512,
    .rxBytesFromISR = serialGenericRxBytesFromISR,
    .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
    .processByte = NULL,
    .data = NULL,
    .enabled = false,
    .flags = 0,
};

extern "C" void USART1_IRQHandler(void) {
    serialGenericUartIRQHandler(&usart1);
}

extern "C" int main(void) {

    // Before doing anything, check if we should enter ROM bootloader
    // enterBootloaderIfNeeded();

    HAL_Init();

    SystemClock_Config();

    SystemPower_Config_ext();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_UCPD1_Init();
    MX_USART1_UART_Init();
    MX_USB_OTG_FS_PCD_Init();
    MX_ICACHE_Init();
    MX_RTC_Init();

    // usbMspInit();

    // Enable the watchdog timer
    // MX_IWDG_Init();

    // rtcInit();

    // Enable hardfault on divide-by-zero
    SCB->CCR |= 0x10;

    // Initialize low power manager
    lpmInit();

    // Inhibit low power mode during boot process
    lpmPeripheralActive(LPM_BOOT);

    BaseType_t rval = xTaskCreate(defaultTask,
                                  "Default",
                                  // TODO - verify stack size
                                  128 * 4,
                                  NULL,
                                  // Start with very high priority during boot then downgrade
                                  // once done initializing everything
                                  2,
                                  NULL);
    configASSERT(rval == pdTRUE);

    // Start FreeRTOS scheduler
    vTaskStartScheduler();

    /* We should never get here as control is now taken by the scheduler */

    while (1){};
}

bool buttonPress(const void *pinHandle, uint8_t value, void *args) {
    (void)pinHandle;
    (void)args;

    printf("Button press! %d\n", value);
    return false;
}

static void udp_rx_cb(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                 const ip_addr_t *addr, u16_t port) {
  (void) arg;
  (void) upcb;
  (void) addr;
  (void) port;

  printf("Received: %s on Port: %d\n", (char*) p->payload, port);

  if (p != NULL) {
    /* free the pbuf */
    pbuf_free(p);
  }
}

static void defaultTask( void *parameters ) {
    (void)parameters;
    struct pbuf* buf = NULL;
    static uint8_t msg[] = "Hello World";
    static ip6_addr_t dst_addr;

    startSerial();
    startSerialConsole(&usart1);
    startCLI();
    serialEnable(&usart1);
    gpioISRStartTask();

    memfault_platform_boot();
    memfault_platform_start();

    bspInit();

    debugSysInit();
    debugMemfaultInit(&usart1);

    // Commenting out while we test usart1
    // lpmPeripheralInactive(LPM_BOOT);

    gpioISRRegisterCallback(&USER_BUTTON, buttonPress);

    printf("Hello from the Default task\n");

    /* Looks like we don't call lwip_init if we are using a RTOS */
    tcpip_init(NULL, NULL);
    // lwip_init();

    /* FIXME: Use Device ID for MAC addr */
    netif.hwaddr[0] =	0x00;
    netif.hwaddr[1] =	0xE0;
    netif.hwaddr[2] =	0x22;
    netif.hwaddr[3] =	0xFE;
    netif.hwaddr[4] =	0xDA;
    netif.hwaddr[5] =	0xC9;
    netif.hwaddr_len = sizeof(netif.hwaddr);

    /* FIXME: Let's not hardcode this if possible */
    ip_addr_t my_addr = IPADDR6_INIT_HOST(0x20010db8, 0x0, 0x0, IPV6_ADDR_LSB);

    /* The documentation says to use tcpip_input if we are running with an OS */
    netif_add(&netif, NULL, bm_l2_init, tcpip_input);

    netif_create_ip6_linklocal_address(&netif, 1);
    netif_set_up(&netif);
    netif_ip6_addr_set(&netif, 0, ip_2_ip6(&my_addr));
    netif_ip6_addr_set_state(&netif, 0, IP6_ADDR_VALID);
    netif_set_link_up(&netif);

    /* Socket API looks... iffy. Using raw udp tx/rx for now */
    udp_pcb = udp_new_ip_type(IPADDR_TYPE_V6);
    udp_port = 2222;
    udp_bind(udp_pcb, IP_ANY_TYPE, udp_port);
    udp_recv(udp_pcb, udp_rx_cb, NULL);
    inet6_aton("ff02::1", &dst_addr);

    while(1) {
        buf = pbuf_alloc(PBUF_TRANSPORT, sizeof(msg), PBUF_RAM);
        memcpy (buf->payload, msg, sizeof(msg));
        udp_sendto_if(udp_pcb, buf, &dst_addr, udp_port, &netif);
        vTaskDelay(100);
        pbuf_free(buf);
    }
}
