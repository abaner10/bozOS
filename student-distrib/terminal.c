#include "keyboard.h"
#include "terminal.h"
#include "x86_desc.h"
#include "lib.h"

/*
 * terminal_open
 *   DESCRIPTION: Opens the terminal driver. Does nothing for now.
 *   INPUTS: filename -- ignored
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- always returns 0
 *   SIDE EFFECTS: none
 */
int32_t terminal_open(const uint8_t* filename) {
    return 0;
}

/*
 * terminal_close
 *   DESCRIPTION: Closes the terminal driver. Does nothing for now.
 *   INPUTS: fd -- ignored
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- always returns 0
 *   SIDE EFFECTS: none
 */
int32_t terminal_close(int32_t fd) {
    return 0;
}

/*
 * terminal_read
 *   DESCRIPTION: Copies data from the intermediate buffer to terminal buffer
 *   INPUTS: fd -- ignored
 *           buf -- pointer to terminal buffer
 *           nbytes -- number of bytes we want to read
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- number of bytes copied, or -1 on failure
 *   SIDE EFFECTS: flushes the intermediate buffer
 */
int32_t terminal_read(int32_t fd, void* buf, int32_t nbytes) {
    // Check for bad inputs
    if (buf == NULL || nbytes < 0) return -1;

    unsigned char* source = get_kb_buffer();
    unsigned char* dest = (unsigned char*) buf;

    int32_t i, bytes_copied, bytes_to_copy, c = 0;
    bytes_to_copy = (nbytes < KB_BUF_SIZE) ? nbytes : KB_BUF_SIZE;
    
    int* enter_flag = kb_read_release();
    while(!(*enter_flag));

    for (i = 0; i < bytes_to_copy; i++) {
        dest[i] = source[i];
        if (source[i] == '\n') break;
        else source[i] = '\0'; // Flush-as-you-go
    }

    *enter_flag = 0; // Reset the keyboard flag
    bytes_copied = i;
    i++;

    // Move the KB buffer 
    while (source[i]!='\0') {
        source[c] = source[i];
        source[i] = '\0';
        c++;
        i++;
    }
    return bytes_copied;
}

/*
 * terminal_write
 *   DESCRIPTION: Writes data from terminal buffer to the screen
 *   INPUTS: fd -- ignored
 *           buf -- pointer to terminal buffer
 *           nbytes -- number of bytes we want to write
 *   OUTPUTS: none
 *   RETURN VALUE: int32_t -- number of bytes written, or -1 on failure
 *   SIDE EFFECTS: flushes the keyboard buffer up until '\n'
 */
int32_t terminal_write(int32_t fd, const void* buf, int32_t nbytes) {
    // Check for bad inputs
    if (buf == NULL || nbytes < 0) return -1;
    
    uint32_t i, bytes_to_write;
    i = 0;
    bytes_to_write = (nbytes < KB_BUF_SIZE) ? nbytes : KB_BUF_SIZE;
    unsigned char* dest = (unsigned char*) buf;

    while (i < bytes_to_write){
        putc(dest[i]);
        if (dest[i] == '\n' || dest[i] == '\0') return (i+1);
        i++;
    }
    return i;
}
