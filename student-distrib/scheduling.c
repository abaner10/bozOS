// This file will include code necessary for scheduling only
// Code for multi-terminal support will be in terminal.c 

#include "i8259.h"
#include "scheduling.h"
#include "IDT.h"
#include "terminal.h"
#include "lib.h"
#include "syscalls.h"

static uint8_t active_task_n;   // Global variable that holds the terminal # where the task is active

/*
 * pit_init
 *   DESCRIPTION: Initializes the Programmable Interrupt Timer (PIT) to tick once every 10ms
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void pit_init() {
    // Source: http://www.osdever.net/bkerndev/Docs/pit.htm
    outb(PIT_INIT_CMD, CMD_REG);        
    outb(SET_FREQ_L, CH0_PORT); // send low byte
    outb(SET_FREQ_H, CH0_PORT); // send high byte
    
    enable_irq(PIT_IRQ_NUM);    // enable IRQ line for PIT
    set_IDT_wrapper(SOFT_INT_START + PIT_IRQ_NUM, pit_handler_asm);
    active_task_n = 0;
}

/*
 * task_switch
 *   DESCRIPTION: Perform task switching. This is also the C interrupt handler for the PIT
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Modifies the ESP, EBP, TSS and paging structure
 */
void task_switch() {
    uint8_t new_task_n = (active_task_n % MAX_TERM_N) + MIN_TERM_N;

    // TODO: Complete this function
    
    // pcb_t* active_task_pcb = 
    // pcb_t* new_task_pcb = 

    // We want to block all other interrupts so that our task switch process is atomic
    active_task_n = new_task_n;
    send_eoi(PIT_IRQ_NUM);  
}
