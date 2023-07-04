//
// Define all task priorities here for ease of access/comparison
// Trying to keep them sorted by priority here
//

#define PCA9535_IRQ_TASK_PRIORITY 20

#define DEFAULT_BOOT_TASK_PRIORITY  16


#define ADIN_SERVICE_TASK_PRIORITY      14
#define BM_L2_TX_TASK_PRIORITY          13
#define BM_DFU_EVENT_TASK_PRIORITY      11

#define GPIO_ISR_TASK_PRIORITY 6

#define BCMP_TASK_PRIORITY	5
#define TIMER_HANDLER_TASK_PRIORITY (5)

#define MIDDLEWARE_NET_TASK_PRIORITY 4

#define USB_TASK_PRIORITY 3
#define BCMP_TOPO_TASK_PRIORITY 3

#define SERIAL_TX_TASK_PRIORITY 2
#define CONSOLE_RX_TASK_PRIORITY 2

#define MIC_TASK_PRIORITY 1

#define CLI_TASK_PRIORITY 1
#define DEFAULT_TASK_PRIORITY 1

#define IWDG_TASK_PRIORITY 0
