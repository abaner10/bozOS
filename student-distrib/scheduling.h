#include "types.h"

#ifndef SCHEDULING_H
#define SCHEDULING_H

/*Magic Numbers*/
#define MAX_TERM_N      3
#define CLOCK_RATE      128     // 100 Hz <--> 10 ms
#define DEFAULT_FREQ    1193182 // From the documentation
#define SET_FREQ_L      ((DEFAULT_FREQ / CLOCK_RATE) & 0xFF)
#define SET_FREQ_H      ((DEFAULT_FREQ / CLOCK_RATE) >> 8)
#define PIT_IRQ_NUM     0
#define CH0_PORT        0x40
#define CMD_REG         0x43
#define PIT_INIT_CMD    0x36    // 0x36: channel 0 output, lobyte/hibyte access, operating mode 3, binary

/*Forward declarations*/
void pit_init();
void task_switch();
uint8_t get_active_task();
void init_shells();
void set_active_task(uint8_t new_task);

volatile uint8_t init_complete;

#endif /*SCHEDULING_H*/
