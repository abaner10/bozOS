#include "syscalls.h"
#include "lib.h"
#include "x86_desc.h"
#include "filesystem.h"
#include "paging.h"
#include "RTC_handler.h"
#include "terminal.h"

/*
 * ----------- Notes for everyone: -----------
 * It seems like CP4 is (mostly) done. Hooray!
 * Before we start working on scheduling for CP5, I'd like everyone to work on cleaning up the code. 
 * This would mean adding necessary comments, reducing unnecessary logic, etc.
 * I'll think of a better way to manage the process_number, hopefully we can have a more robust approach. 
 * 
 * Some other things that need to be done:
 * 1) Add support for function keys so we can use multiple terminals. Also the 'TAB' key prints weird characters, that needs to be fixed.
 * 2) Redo the FOTP setup as per Andrew's recommendation. 
 * 
 * Have a good break. 
 * Sean 11/17/17
 * --------------------------------------------
 */

/* File Operations Table Pointers */
// TODO: Andrew Sun suggested using a struct instead of this array. Kush, you I'd suggest you look into this.
// Also replace 'NULL' with a generic function that does nothing. 
generic_fp* stdin_fotp[4] = {(generic_fp*) terminal_open, (generic_fp*) terminal_read, NULL, (generic_fp*) terminal_close};
generic_fp* stdout_fotp[4] = {(generic_fp*) terminal_open, NULL, (generic_fp*) terminal_write, (generic_fp*) terminal_close};

generic_fp* file_fotp[4] = {(generic_fp*) fopen, (generic_fp*) fread, (generic_fp*) fwrite,(generic_fp*) fclose};
generic_fp* dir_fotp[4] = {(generic_fp*) dopen, (generic_fp*) dread, (generic_fp*) dwrite, (generic_fp*) dclose};
generic_fp* rtc_fotp[4] = {(generic_fp*) rtc_open, (generic_fp*) rtc_read, (generic_fp*) rtc_write, (generic_fp*) rtc_close};

/*
 * halt
 *   DESCRIPTION: Handler for 'halt' system call.
 *   INPUTS: status -- ???
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- 0 on success, -1 on failure
 *   SIDE EFFECTS: none
 */
int32_t halt(uint8_t status) {
    // Store ESP and EBP of the parent process, we can call a normal ret
    // Then we can resume at the parent program where we left off

    // printf("System call HALT.\n");
    uint8_t i;
    uint32_t status_32 = status;
    
    process_number--;
    // We cannot close the base shell
    if (process_number <= 0) {
        process_number = 0;
        // Call execute again with all values reinitialized
        clear();
        execute((uint8_t*) "shell");
    }

    // We subtract -1 to get the parent process. 
    // This will need to be changed for subsequent checkpoints when we use an array/struct to keep track of our processes.
    uint32_t pcb_addr = KERNEL_BASE - process_number * PCB_OFFSET;  // find where program stack starts
    pcb_t* PCB_base_parent = (pcb_t*) pcb_addr;                     // cast it to PCB so start

    pcb_addr = KERNEL_BASE - (process_number+1) * PCB_OFFSET;       // find where program stack starts
    pcb_t* PCB_base_self = (pcb_t*) pcb_addr;                       // cast it to PCB so start

    /* Restore parent's paging */
    uint32_t parent_physical_mem = PCB_base_parent->self_page;
    page_directory[(USER_MEM_V >> ALIGN_4MB)] = parent_physical_mem | USER_PAGE_SET_BITS;

    // Tadas pointed out that we don't need to reload page_directory into CR3
    // Flush the TLB (flushing happens whenever we reload CR3)
    asm volatile(
        "movl %%cr3, %%eax;"
        "movl %%eax, %%cr3;"
        : /* no outputs */
        : /* no inputs */
        : "eax"
    );

    // close relevant FDs
    fd_t* fd_array = PCB_base_self->fd_arr;
    for (i = 0 ; i < MAX_FILES ; i++) {
        if(fd_array[i].in_use_flag == FILE_IN_USE){
            fd_array[i].fotp = NULL;
            fd_array[i].inode_number = 0;
            fd_array[i].file_position = 0;
            fd_array[i].in_use_flag = FILE_NOT_IN_USE;
        }
    }

    // Restore parent ESP0
    tss.ss0 = KERNEL_DS;                        // Segment selector: we don't actually have to do this
    tss.esp0 = PCB_base_parent->self_k_stack;   // restore to pointer to parent_k_stack

    asm volatile(
        "movl %0, %%esp;"
        "movl %1, %%ebp;"
        "movl %2, %%eax;"
        "jmp SYS_HALT_RETURN_POINT;"
        : /*no outputs*/
        : "r" (PCB_base_self->self_esp), "r" (PCB_base_self->self_ebp), "r" (status_32)
        : "esp", "ebp"
    );

    // We should never reach here
    return status;
}

/*
 * execute
 *   DESCRIPTION: Handler for 'execute' system call.
 *   INPUTS: command -- space-separated sequence of words
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- 0 on success, -1 on failure
 *   SIDE EFFECTS: executes the user program given by 'command' input
 */
int32_t execute(const uint8_t* command) {
    // printf("System call EXECUTE.\n");
    uint8_t i, nbytes, cmd1[KB_BUF_SIZE], exe_buf[BYTES_4], entry_pt_buf[BYTES_4];
    uint8_t * data_buf;
    uint32_t entry_pt_addr, user_prog_physical_mem, pcb_addr, new_esp0, self_ebp, self_esp;
    dentry_t cmd_dentry;
    pcb_t* PCB_base;
    fd_t* fd_array;

    uint16_t user_ds_addr16 = USER_DS;
    uint32_t user_ds_addr32 = USER_DS;
    uint32_t user_stack_addr = USER_STACK;
    uint32_t int_flag_bitmask = INT_FLAG;
    uint32_t user_cs_addr32 = USER_CS;

    /*********** Step 1: Parse arguments ***********/
    // 'command' is a space-separated sequence of words
    i = 0;
    nbytes = 0;
    memset(cmd1, '\0', KB_BUF_SIZE); // Set buffer for first command word to be null-terminated

    while (i < KB_BUF_SIZE) {
        if (command[i] == '\n' || command[i] == '\0' || command[i] == ' ') {
            nbytes = i;
            strncpy((int8_t*) cmd1, (int8_t*) command, nbytes);
            break;
        }
        i++;
    }
    // Edge case: We need to copy the entire command string to cmd1
    if (i == KB_BUF_SIZE) {
        strncpy((int8_t*) cmd1, (int8_t*) command, KB_BUF_SIZE);
    }
    // TODO: Employ getargs here, probably need to use 'nbytes' as a starting point/offset


    /*********** Step 2: Check file validity ***********/
    // Check if the file can be read or not
    if (read_dentry_by_name((uint8_t*) cmd1, &cmd_dentry) == -1) return -1;

    // Check if the file can be executed or not
    if (read_data(cmd_dentry.inode, 0, exe_buf, BYTES_4) == -1) return -1;
    // The first 4 bytes of the file represent a series of "magic numbers" that identify the file as executable 
    if (exe_buf[0] != EXE_BYTE0) return -1;
    if (exe_buf[1] != EXE_BYTE1) return -1;
    if (exe_buf[2] != EXE_BYTE2) return -1;
    if (exe_buf[3] != EXE_BYTE3) return -1;

    // Get the entry point from bytes 24-27 of the executable
    if (read_data(cmd_dentry.inode, ENTRY_PT_OFFSET, entry_pt_buf, BYTES_4) == -1) return -1;

    entry_pt_addr = 0;
    for (i = 0; i < BYTES_4; i++) {
        // Sanity check: The entry point address should be somewhere near 0x08048000 (see Appendix C)
        // The order of bits in entry_pt_addr is [27-26-25-24]
        entry_pt_addr = entry_pt_addr | (entry_pt_buf[i] << SHIFT_8*i);
    }

    /*********** Step 3: Set up paging ***********/
    // 'page_directory' is defined in paging.h
    // We map virtual address USER_MEM_V (128 MiB) to physical address USER_MEM_P + (process #) * 4 MiB
    user_prog_physical_mem = USER_MEM_P + process_number * USER_PROG_SIZE;
    page_directory[(USER_MEM_V >> ALIGN_4MB)] = user_prog_physical_mem | USER_PAGE_SET_BITS;

    // Tadas pointed out that we don't need to reload page_directory into CR3
    // Flush the TLB (flushing happens whenever we reload CR3)
    asm volatile(
        "movl %%cr3, %%eax;"
        "movl %%eax, %%cr3;"
        : /* no outputs */
        : /* no inputs */
        : "eax"
    );

    /*********** Step 4: Load file into memory ***********/
    // The program image must be copied to the correct offset (0x48000) within that page
    data_buf = (uint8_t*) USER_PROG_LOC; // We probably don't need an array data_buf, instead we can cast an address to a pointer
    if (read_data(cmd_dentry.inode, 0, data_buf, USER_PROG_SIZE) == -1) return -1;


    /*********** Step 5: Create PCB / open FDs ***********/
    // TODO: Limit # of processes to 6.
    
    // Find where program stack starts. we do '+1' here as we only increment process_number below
    // We can simply cast the address of the program's kernel stack to be a pcb_t pointer. No need to use memcpy.
    pcb_addr = KERNEL_BASE - (process_number+1)*PCB_OFFSET;     // find where program stack starts
    PCB_base = (pcb_t*) pcb_addr;                               // cast it to PCB so start of program stack contains PCB.
    new_esp0 = KERNEL_BASE - (process_number * PCB_OFFSET) - BYTES_4;

    PCB_base->status = TASK_RUNNING;
    PCB_base->pid = process_number;                 // Process ID
    PCB_base->self_k_stack = new_esp0;              // Store it's own kernel stack
    PCB_base->self_page = user_prog_physical_mem;   // Store it's own user stack

    fd_array = PCB_base->fd_arr;
    for (i = 0 ; i < MAX_FILES ; i++) {  // initalize file descriptor array
        fd_array[i].fotp = NULL;
        fd_array[i].inode_number = 0;
        fd_array[i].file_position = 0;
        fd_array[i].in_use_flag = FILE_NOT_IN_USE;
    }

    // Save ESP and EBP
    asm volatile(
        "movl %%esp, %0;"
        "movl %%ebp, %1;"
        : "=r" (self_esp), "=r" (self_ebp)
    );
    PCB_base->self_esp = self_esp;
    PCB_base->self_ebp = self_ebp;

    // start stdin process
    PCB_base->fd_arr[0].fotp = (generic_fp*) stdin_fotp; // TABLE FOR STDIN
    PCB_base->fd_arr[0].inode_number = 0; // NOT A DATA File
    PCB_base->fd_arr[0].file_position = 0;
    PCB_base->fd_arr[0].in_use_flag = FILE_IN_USE;
    strncpy((int8_t*) PCB_base->fd_arr[0].arg, (int8_t*) cmd2,arg_nbytes);

    // check if file is a textfile
    if (strlen((int8_t*) cmd2) > MIN_NAME_TEXT) {
        if (strncmp((int8_t*) (cmd2 + (strlen((int8_t*) cmd2) - MIN_NAME_TEXT)), TXT , MIN_NAME_TEXT) == 0) {
            PCB_base->fd_arr[1].text_file_flag = 1;
        }
        else {
            PCB_base->fd_arr[1].text_file_flag = 0;
        }
    }
    else {
        PCB_base->fd_arr[1].text_file_flag = 0;
    }


    // start stdout process
    PCB_base->fd_arr[1].fotp = (generic_fp*) stdout_fotp; // TABLE FOR STDOUT
    PCB_base->fd_arr[1].inode_number = 0; // NOT A DATA File
    PCB_base->fd_arr[1].file_position = 0;
    PCB_base->fd_arr[1].in_use_flag = FILE_IN_USE; 

    process_number++;

    /*********** Step 6: Set up IRET context ***********/
    /* The only things that really change here upon each syscall are: 1) tss.esp0, 2) ESP-on-stack (in IRET context), 3) page table
     *
     * The IRET instruction expects, when executed, the stack to have the following contents
     * (starting from the stack pointer - lowermost address upwards):
     * 1. The instruction to continue execution at - the value of EIP (pointer to our function entry point).
     * 2. The code segment (CS) selector to change to.
     * 3. The value of the EFLAGS register to load.
     * 4. The stack pointer to load.
     * 5. The stack segment (SS) selector to change to.
     *
     * The stack prior to IRET looks like:
     * ----------
     *    EIP      <-- Bytes 24-27 of the executable we load
     * ----------
     *     CS      <-- User-mode Code Segment
     * ----------
     *   EFLAGS
     * ----------
     *    ESP      <-- User-mode ESP
     * ----------
     *     SS      <-- User-mode Stack Segment
     * ----------
     *
     * Sources:
     * https://stackoverflow.com/questions/6892421/switching-to-user-mode-using-iret
     * http://jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
     * http://x86.renejeschke.de/html/file_module_x86_id_145.html
     * http://www.felixcloutier.com/x86/IRET:IRETD.html
     * http://wiki.osdev.org/Getting_to_Ring_3
     * http://wiki.osdev.org/System_Calls
     * http://wiki.osdev.org/Task_State_Segment
     * http://wiki.osdev.org/Context_Switching
     */

    tss.ss0 = KERNEL_DS; // Segment selector
    tss.esp0 = new_esp0; // New user program's kernel stack. Starts at (8MB - 8KB) for process #0, (8MB - 8KB - 8KB) for process #1

    // Push IRET context to stack
    asm volatile(
        "cli;"                  /* Context-switch is critical, so we suppress interrupts */

        // "movw %1, %%ss;"      /* Code-segment */ The IRET below will update %ss for us
        "movw %0, %%ds;"      /* Data-segment */
        "movw %0, %%es;"      /* Additional data-segment register */
        "movw %0, %%fs;"      /* Additional data-segment register */
        "movw %0, %%gs;"      /* Additional data-segment register */
        : /*no outputs*/
        : "r" (user_ds_addr16)
    );

    asm volatile(
        "pushl %0;"         /* Push USER_DS */
        "pushl %1;"         /* Push USER_STACK pointer */

        "pushf;"            /* Push EFLAGS onto the stack */
        "popl %%eax;"       /* Get EFLAGS back into EAX. The only way to read EFLAGS is to pushf then pop */
        "orl %2, %%eax;"    /* Set the IF flag (same thing as STI; we use this because calling STI will cause a pagefault) */
        "pushl %%eax;"      /* Push the new EFLAGS value back onto the stack */

        "pushl %3;"
        "pushl %4;"         /* User program/function entry point */
        "iret;"
        "SYS_HALT_RETURN_POINT: ;"

        : /*no outputs*/
        : "r" (user_ds_addr32), "r" (user_stack_addr), "r" (int_flag_bitmask), "r" (user_cs_addr32), "r" (entry_pt_addr)
        : "eax"
    );

    return 0;
}

/*
 * read
 *   DESCRIPTION: Handler for 'read' system call.
 *   INPUTS: fd -- file descriptor to index into fd array
 *           buf -- buffer to read from
 *           nbytes -- number of bytes to read
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- 0 on success, -1 on failure
 *   SIDE EFFECTS: none
 */
int32_t read (int32_t fd, void* buf, int32_t nbytes) {
    // This function is called within a given user program.
    // Based on the file descriptor #, we index into the PCB's FD array and find the relevant 'file operations table pointer'
    // in the read you look for the fd file in the fd_arr, then
    // use the operations pointer to get the function

    // Check for invalid inputs
    pcb_t* PCB_base = get_PCB_base(process_number);

    if (PCB_base == NULL || PCB_base >= (pcb_t*) USER_MEM_P) return -1;
    if (buf == NULL || fd < 0 || fd > MAX_FILES-1 || nbytes < 0) return -1;
    if (PCB_base->fd_arr[fd].in_use_flag == FILE_NOT_IN_USE) return -1;

    if (PCB_base->fd_arr[fd].fotp != NULL && PCB_base->fd_arr[fd].fotp[FOTP_READ]) {
        return (PCB_base->fd_arr[fd].fotp[FOTP_READ])(fd, buf, nbytes);
    }
    return -1;

    //NOTE: .fileName in the struct is just there so that this function can return 0... filname
}

/*
 * write
 *   DESCRIPTION: Handler for 'write' system call.
 *   INPUTS: fd -- file index to write to
 *           buf -- buffer to write to
 *           nbytes -- Number of bytes to write
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- 0 on success, -1 on failure
 *   SIDE EFFECTS: none
 */
int32_t write (int32_t fd, const void* buf, int32_t nbytes) {
    // printf("System call WRITE.\n");
    // if file's buffer is NULLL or fd is nto in range then we return -1
    pcb_t* PCB_base = get_PCB_base(process_number);
    
    // Check for invalid inputs
    if (PCB_base == NULL || PCB_base >= (pcb_t*) USER_MEM_P) return -1;
    if (buf == NULL || fd < 0 || fd > MAX_FILES-1 || nbytes < 0) return -1;
    
    // if file has never been opened we return -1
    if (PCB_base->fd_arr[fd].in_use_flag == FILE_NOT_IN_USE) return -1;

    else if (PCB_base->fd_arr[fd].fotp[FOTP_WRITE] != NULL) {
        return (PCB_base->fd_arr[fd].fotp[FOTP_WRITE])(fd, buf, nbytes);
    }
    return -1;
}

/*
 * open
 *   DESCRIPTION: Handler for 'open' system call.
 *   INPUTS: filename -- filename to be opened
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- 0 on success, -1 on failure
 *   SIDE EFFECTS: sets the in_use_flag of the file
 */
int32_t open (const uint8_t* filename) {
    // printf("System call OPEN.\n");
    // This function is called within a given user program.
    // Finds the first 'fd' that is not in use and opens the file and puts it there
    // by setting the appropriate inode numbers!
    pcb_t* PCB_base = get_PCB_base(process_number);
    // Check for invalid inputs
    if (PCB_base == NULL || PCB_base >= (pcb_t*) USER_MEM_P) return -1;

    dentry_t file_dentry;
    int32_t i = 0, fd = -1;
    if (read_dentry_by_name(filename, &file_dentry) == -1) return -1;

    // find the fd that is not in use
    for (i = 0; i < MAX_FILES; i++) { // you always traverse 0 --> 7 
        if (PCB_base->fd_arr[fd].in_use_flag != FILE_IN_USE) {
            fd = i;
            break; // we found the first entry which is not in use!
        }
    }
    if (fd > MAX_FILES-1 || fd == -1) return -1; // if there is no space to open a file. According to the spec, we are only allowed 6 opened files

    if (file_dentry.fileType == _DIR_) {
        if (dopen(filename, &file_dentry) != 0) return -1;
        PCB_base->fd_arr[fd].inode_number = NULL;
        PCB_base->fd_arr[fd].file_position++;
        PCB_base->fd_arr[fd].fotp = (generic_fp*) dir_fotp;
        PCB_base->fd_arr[fd].in_use_flag = FILE_IN_USE;
    }
    else if (file_dentry.fileType == _FILE_) {
        if (fopen(filename) != 0) return -1;
        PCB_base->fd_arr[fd].inode_number = file_dentry.inode;
        PCB_base->fd_arr[fd].file_position++;
        PCB_base->fd_arr[fd].fotp = (generic_fp*) file_fotp;
        PCB_base->fd_arr[fd].in_use_flag = FILE_IN_USE;
    }
    else if (file_dentry.fileType == _RTC_) {
        if (rtc_open(filename) != 0) return -1;
        PCB_base->fd_arr[fd].inode_number = NULL;
        PCB_base->fd_arr[fd].file_position++;
        PCB_base->fd_arr[fd].fotp = (generic_fp*) rtc_fotp;
        PCB_base->fd_arr[fd].in_use_flag = FILE_IN_USE;
    }
    else return -1; // We cannot understand the file type..
    return 0; //success
}

/*
 * close
 *   DESCRIPTION: Handler for 'close' system call.
 *   INPUTS: fd -- file descriptor to index into fd array
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- 0 on success, -1 on failure
 *   SIDE EFFECTS: none
 */
int32_t close (int32_t fd) {
    // printf("System call CLOSE.\n");
    // This function is called within a given user program.
    // Finds the corredsponding fd and sets all its elements in the struct equal to nothing
    pcb_t* PCB_base = get_PCB_base(process_number);
    // Check for invalid inputs
    if (PCB_base == NULL || PCB_base >= (pcb_t*) USER_MEM_P) return -1;
    if (fd == 0 || fd == 1 || fd < 0 || fd > MAX_FILES-1) return -1;

    if (PCB_base->fd_arr[fd].in_use_flag != FILE_IN_USE) return -1; // WRONG fd given

    // check if I can close the file!
    if ((PCB_base->fd_arr[fd].fotp[FOTP_CLOSE])(fd) != 0) return -1;

    // set the flag to not in use
    PCB_base->fd_arr[fd].in_use_flag = FILE_NOT_IN_USE;

    return 0; // return success
}

/*
 * getargs
 *   DESCRIPTION: Handler for 'getargs' system call.
 *   INPUTS: buf -- ???
 *           nbytes -- ???
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- 0 on success, -1 on failure
 *   SIDE EFFECTS: none
 */
int32_t getargs (uint8_t* buf, int32_t nbytes) {
    // printf("System call GETARGS.\n");

    // get the stdin argument which is fd_0
    pcb_t* PCB_base = get_PCB_base(process_number);
    if (PCB_base == NULL || PCB_base >= (pcb_t*) USER_MEM_P) return -1;

    if (PCB_base->fd_arr[0].arg == NULL) return -1;
    // clear the buffer
    memset(buf,'\0',BUF_SIZE);
    memcpy(buf,PCB_base->fd_arr[0].arg,nbytes);
    return 0;
}

/*
 * vidmap
 *   DESCRIPTION: Maps the text-mode video memory into user space at a pre-set virtual address. 
 *   INPUTS: screen_start -- user-space pointer to video memory
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- 0 on success, -1 on failure
 *   SIDE EFFECTS: modifies paging structure
 */
int32_t vidmap (uint8_t** screen_start) {
    // printf("System call VIDMAP.\n");    
    // Check for bad pointers
    uint32_t screen_start_casted = (uint32_t) screen_start;
    if (screen_start_casted == 0x0) return -1;
    if (screen_start_casted >= KERNEL_TOP && screen_start_casted < KERNEL_BASE) return -1;

    // Set up new user-level paging
    page_directory[(USER_VIDEO_MEM >> ALIGN_4MB)] = ((uint32_t) vidmap_ptable) | 0x7; // 4 KiB page, user access, r/w access, present
    
    uint16_t i;
    for (i = 0; i < PAGE_SIZE; i++) {
        vidmap_ptable[i] = 0x6; // 4 KiB page, user access, r/w access, not present
    }
    vidmap_ptable[0] = (VIDEO_MEM) | 0x7; // 4 KiB page, user access, r/w access, present

    // Flush the TLB
    asm volatile(
        "movl %%cr3, %%eax;"
        "movl %%eax, %%cr3;"
        : /*no outputs*/
        : /*no inputs*/
        : "eax"
    );

    // Finally: Reassign screen_start to point to the USER_VIDEO_MEMORY
    // The idea is to assign some address outside the already allocated space to be used as a pointer to video memory
    // Sean: I choose 136 MiB as a starting point
    *screen_start = (uint8_t*) (USER_VIDEO_MEM);
    return 0;
}
/*
 * set_handler
 *   DESCRIPTION: Handler for 'set_handler' system call.
 *   INPUTS: signum -- ???
 *           handler -- ???
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- 0 on success, -1 on failure
 *   SIDE EFFECTS: none
 */
int32_t set_handler (int32_t signum, void* handler) {
    printf("System call SET_HANDLER.\n");
    /******************* EXTRA CREDIT *************************/

    return 0;
}

/*
 * sigreturn
 *   DESCRIPTION: Handler for 'sigreturn' system call.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- 0 on success, -1 on failure
 *   SIDE EFFECTS: none
 */
int32_t sigreturn (void) {
    printf("System call SIGRETURN.\n");
    /******************* EXTRA CREDIT *************************/

    return 0;
}

/*
 * get_PCB_base
 *   DESCRIPTION: Returns the PCB base pointer of the CURRENT process
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: pcb_t -- pointer to the topmost PCB on the kernel stack
 *   SIDE EFFECTS: none
 */
pcb_t* get_PCB_base(int8_t process_num) {
    if (process_num < 0 || process_num >= MAX_PROCESSES) return NULL;

    // We don't do -1 here because process_num here represents the # of the NEXT available process
    uint32_t pcb_addr = KERNEL_BASE - process_num * PCB_OFFSET; // find where program stack starts
    pcb_t* PCB_base = (pcb_t*) pcb_addr; // cast it to PCB so start of program stack contains PCB.

    return PCB_base;
}
