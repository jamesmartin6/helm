#ifndef HELM_UART_H
#define HELM_UART_H

#include <stdint.h>

/* Driver for the CMSDK APB UART0 peripheral QEMU exposes on the mps2-an385
 * machine at 0x40004000. QEMU bridges this to a host PTY when run with
 * `-serial pty`, so the backend/harness can open it like a real serial port. */

void uart_init(void);

/* Blocking single-byte write (busy-waits while the TX buffer is full). */
void uart_putc(uint8_t byte);

/* Non-blocking read: returns 1 and writes *out if a byte is available,
 * else returns 0 immediately. */
int uart_try_getc(uint8_t *out);

#endif /* HELM_UART_H */
