#include "lib.h"
#include "IDT.h"
#include "x86_desc.h"
#include "syscalls.h"

/* Functions to handle exceptions
 * Source: http://www.osdever.net/bkerndev/Docs/isrs.htm
 */

/* Function pointer array for exceptions */
void (*handle_exceptions_arr[N_EXCEPTIONS])() = {handle_e0_asm, handle_e1_asm, handle_e2_asm, handle_e3_asm, handle_e4_asm, handle_e5_asm, handle_e6_asm, handle_e7_asm, handle_e8_asm, handle_e9_asm, handle_e10_asm, handle_e11_asm, handle_e12_asm, handle_e13_asm, handle_e14_asm, handle_e15_asm, handle_e16_asm, handle_e17_asm, handle_e18_asm, handle_e19_asm};

/*
 * set_IDT_wrapper
 *   DESCRIPTION: Sets up a given IDT entry, along with the interrupt handler function.
 *                Sets the reserved bits to '01110' (corresponds to 14), along with the
 *                DPL, segment selector and marks the entry as present.
 *   INPUTS: idt_num -- the index of IDT we want to refer to
 *           handler_function -- function pointer to handle the given interrupt
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: modifies entries in the IDT
 */
void set_IDT_wrapper(uint8_t idt_num, void* handler_function) {
    SET_IDT_ENTRY(idt[idt_num], handler_function);
    idt[idt_num].reserved4 = 0;
    idt[idt_num].reserved3 = 1;
    idt[idt_num].reserved2 = 1;
    idt[idt_num].reserved1 = 1;
    idt[idt_num].reserved0 = 0;
    idt[idt_num].present = 1;
    idt[idt_num].size = 1;
    idt[idt_num].dpl = 0;
    idt[idt_num].seg_selector = KERNEL_CS;
}


/*
 * init_IDT
 *   DESCRIPTION: Main function to initialize the IDT with exception/interrupt handlers.
 *                Calls the helper function set_IDT_wrapper()
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: modifies entries in the IDT
 */
void init_IDT() {
    unsigned int i;
    // Initalizes the exceptions by writing them to the IDT
    for (i = 0; i < N_EXCEPTIONS_RESERVED; i++) {
        if (i < N_EXCEPTIONS) {
            set_IDT_wrapper(i, handle_exceptions_arr[i]);
        }
        else {
            // Indices 20-31 are reserved for some other purpose (according to the spec), so we write a default handler
            set_IDT_wrapper(i, handle_default_asm);
        }
    }

    /* Initialize user-defined interrupts, which start at IDT entry 32 'USER_INT_START'.
     * Source: https://courses.engr.illinois.edu/ece391/fa2017/secure/references/descriptors.pdf
     * According to spec, there are 15 default hardware interrupts.
     * Source: http://wiki.osdev.org/Interrupts#General_IBM-PC_Compatible_Interrupt_Information
     */

    for (i = USER_INT_START; i < NUM_VEC; i++ ) {
        set_IDT_wrapper(i, handle_default);
        idt[i].present = 0; // Mark as 'not present' unless otherwise stated

        if (i == SYS_CALL_ADDR) {
            // System call
            set_IDT_wrapper(i, handle_syscall_asm);
            idt[i].dpl = 3; // System call should have its DPL set to 3 so that it is accessible from user space via the 'int' instruction
            idt[i].seg_selector = KERNEL_CS;
        }
    }
}

/*
 * print_error_code
 *   DESCRIPTION: Prints the error code to the screen.
 *                Used only by exceptions  #8, #11, #12, #13, #14, #17, #30
 *   INPUTS: code -- error code
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: prints error code to the screen
 */
void print_error_code(uint32_t code) {
    printf("Error code (hex): %x\n", code);
}


/*
 * handle_e0
 *   DESCRIPTION: Handler for exception #0
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e0() {
    printf("Interrupt 0 - Divide Error Exception (#DE) \n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e1
 *   DESCRIPTION: Handler for exception #1
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e1() {
    printf("Interrupt 1 - Debug Exception (#DB) \n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e2
 *   DESCRIPTION: Handler for exception #2
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e2() {
    printf("Interrupt 2 - NMI Interrupt\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e3
 *   DESCRIPTION: Handler for exception #3
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e3() {
    printf("Interrupt 3 - Breakpoint Exception (#BP)\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e4
 *   DESCRIPTION: Handler for exception #4
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e4() {
    printf("Interrupt 4 - Overflow Exception (#OF)\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e5
 *   DESCRIPTION: Handler for exception #5
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e5() {
    printf("Interrupt 5 - BOUND Range Exceeded Exception (#BR)\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e6
 *   DESCRIPTION: Handler for exception #6
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e6() {
    printf("Interrupt 6 - Invalid Opcode Exception (#UD)\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e7
 *   DESCRIPTION: Handler for exception #7
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e7() {
    printf("Interrupt 7 - Device Not Available Exception (#NM)\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e8
 *   DESCRIPTION: Handler for exception #8
 *   INPUTS: error_code -- error code to print to the terminal
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e8(uint32_t error_code) {
    printf("Interrupt 8 - Double Fault Exception (#DF)\n");
    printf("Error code (hex): %x\n", error_code);
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e9
 *   DESCRIPTION: Handler for exception #9
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e9() {
    // Exception Class Abort.
    // Intel reserved; do not use. Recent IA-32 processors do not generate this exception.
    printf("Interrupt 9 - Coprocessor Segment Overrun\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e10
 *   DESCRIPTION: Handler for exception #10
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e10() {
    printf("Interrupt 10 - Invalid TSS Exception (#TS)\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e11
 *   DESCRIPTION: Handler for exception #11
 *   INPUTS: error_code -- error code to print to the terminal
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e11(uint32_t error_code) {
    printf("Interrupt 11 - Segment Not Present (#NP)\n");
    printf("Error code (hex): %x\n", error_code);
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e12
 *   DESCRIPTION: Handler for exception #12
 *   INPUTS: error_code -- error code to print to the terminal
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e12(uint32_t error_code) {
    printf("Interrupt 12 - Stack Fault Exception (#SS)\n");
    printf("Error code (hex): %x\n", error_code);
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e13
 *   DESCRIPTION: Handler for exception #13
 *   INPUTS: error_code -- error code to print to the terminal
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e13(uint32_t error_code) {
    printf("Interrupt 13 - General Protection Exception (#GP)\n");
    printf("Error code (hex): %x\n", error_code);
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e14
 *   DESCRIPTION: Handler for exception #14
 *   INPUTS: error_code -- error code to print to the terminal
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e14(uint32_t error_code) {
    // Grab CR2 register (tells us the page fault linear address)
    uint32_t addr;
    asm volatile(
        "movl %%cr2, %0"
        : "=r" (addr)
    );
    printf("Interrupt 14 - Page-Fault Exception (#PF) at address 0x%x\n", addr);
    printf("Error code (hex): %x\n", error_code);
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e15
 *   DESCRIPTION: Handler for exception #15
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e15() {
    printf("Interrupt 15 - Reserved\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e16
 *   DESCRIPTION: Handler for exception #16
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e16() {
    printf("Interrupt 16 - x87 FPU Floating-Point Error (#MF)\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e17
 *   DESCRIPTION: Handler for exception #17
 *   INPUTS: error_code -- error code to print to the terminal
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e17(uint32_t error_code) {
    printf("Interrupt 17 - Alignment Check Exception (#AC)\n");
    printf("Error code (hex): %x\n", error_code);
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e18
 *   DESCRIPTION: Handler for exception #18
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e18() {
    printf("Interrupt 18 - Machine-Check Exception (#MC)\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_e19
 *   DESCRIPTION: Handler for exception #19
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_e19() {
    printf("Interrupt 19 - SIMD Floating-Point Exception (#XF)\n");
    halt(PROG_DIED_BY_EXCEPTION);
}

/*
 * handle_default
 *   DESCRIPTION: Handler for default exceptions (#20-#31)
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: void
 *   SIDE EFFECTS: masks interrupts, halts system
 */
void handle_default() {
    printf("Default interrupt handler called. Nothing specified here.\n");
    halt(PROG_DIED_BY_EXCEPTION);
}
