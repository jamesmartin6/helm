#include "uart.h"

#define UART0_BASE 0x40004000u

#define UART0_DATA       (*(volatile uint32_t *)(UART0_BASE + 0x00u))
#define UART0_STATE      (*(volatile uint32_t *)(UART0_BASE + 0x04u))
#define UART0_CTRL       (*(volatile uint32_t *)(UART0_BASE + 0x08u))
#define UART0_INTSTATUS  (*(volatile uint32_t *)(UART0_BASE + 0x0Cu))
#define UART0_BAUDDIV    (*(volatile uint32_t *)(UART0_BASE + 0x10u))

#define STATE_TXFULL     (1u << 0)
#define STATE_RXFULL     (1u << 1)

#define CTRL_TXEN        (1u << 0)
#define CTRL_RXEN        (1u << 1)

void uart_init(void) {
    /* Minimum valid divider per the CMSDK UART model; QEMU's chardev I/O is
     * immediate regardless (no real bit-clock timing), this just satisfies
     * the peripheral's own sanity check. */
    UART0_BAUDDIV = 16u;
    UART0_CTRL = CTRL_TXEN | CTRL_RXEN;
}

void uart_putc(uint8_t byte) {
    while (UART0_STATE & STATE_TXFULL) {
        /* busy-wait for the single-byte TX buffer to drain */
    }
    UART0_DATA = byte;
}

int uart_try_getc(uint8_t *out) {
    if (UART0_STATE & STATE_RXFULL) {
        *out = (uint8_t)UART0_DATA;
        return 1;
    }
    return 0;
}
