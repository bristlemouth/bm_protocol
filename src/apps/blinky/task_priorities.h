//
// Define all task priorities here for ease of access/comparison
// Trying to keep them sorted by priority here
//

#define PCA9535_IRQ_TASK_PRIORITY 20

#define DEFAULT_BOOT_TASK_PRIORITY 16

#define GPIO_ISR_TASK_PRIORITY 6

#define USB_TASK_PRIORITY 3

#define SERIAL_TX_TASK_PRIORITY 2
#define CONSOLE_RX_TASK_PRIORITY 2

#define CLI_TASK_PRIORITY 1
#define DEFAULT_TASK_PRIORITY 1

#define IWDG_TASK_PRIORITY 0
