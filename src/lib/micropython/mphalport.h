#include "mpconfigport.h"

void mp_hal_rx_to_buff(uint8_t byte);
void mp_hal_init(void *serial_handle);
mp_uint_t mp_hal_ticks_ms(void);
void mp_hal_set_interrupt_char(int c);
void mp_hal_delay_ms(mp_uint_t ms);
int mp_hal_stdin_rx_chr(void);
void mp_hal_process_byte(void *serialHandle, uint8_t byte);
