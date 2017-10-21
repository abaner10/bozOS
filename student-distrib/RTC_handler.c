#include "lib.h"
#include "x86_desc.h"
#include "RTC_handler.h"
#include "i8259.h"
/*http://wiki.osdev.org/RTC*/
void rtc_init(void){
  /*Not entirely sure what this means???
  Be sure that you install the IRQ handler
  before you enable the RTC IRQ.
  The interrupt will happen almost immediately.*/
  //cli();
  char  previous;
  outb(0x8B, 0x70); //select status register B and disable interuppts using x80.
  previous = inb(0x71); //	  read immediately after or the RTC may be left in an unknown state.
  outb(0x8B, 0x70);
  outb(previous | 0x40, 0x71);  // write the previous value ORed with 0x40. This turns on bit 6 of register B
  enable_irq(8); //enable 8th IRQ
  SET_IDT_ENTRY(idt[32+8], rtc_handler);

  /*sti();*/
  outb(0x0B, 0x70);  //enable 80 bit NMI?? IDK
}

void rtc_handler(void){
    printf("RTC handler call");
  test_interrupts(); //we must prove this function is being called.
  send_eoi(8); //end 8th IRQ
  outb(0x0C, 0x70);	// select register C
  inb(0x71);		// just throw away contents, we must do this otherwise IRQ8 will never be called again.
}