/*
 * Startup code for the mps2-an385 (Cortex-M3) target: vector table + reset
 * handler. Adapted from FreeRTOS's own CORTEX_MPS2_QEMU_IAR_GCC demo
 * startup_gcc.c. QEMU's `-kernel` ELF loader writes .data's initial values
 * directly into RAM, so (unlike real hardware) Reset_Handler doesn't need a
 * manual flash->RAM copy step before calling main().
 */

#include <stdint.h>

extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);

static void HardFault_Handler(void) __attribute__((naked));
static void Default_Handler(void);
void Reset_Handler(void) __attribute__((naked));

extern int main(void);
extern uint32_t _estack;

const uint32_t *isr_vector[] __attribute__((section(".isr_vector"), used)) = {
    (uint32_t *)&_estack,
    (uint32_t *)&Reset_Handler,      /* Reset                -15 */
    (uint32_t *)&Default_Handler,    /* NMI                  -14 */
    (uint32_t *)&HardFault_Handler,  /* HardFault             -13 */
    (uint32_t *)&Default_Handler,    /* MemManage             -12 */
    (uint32_t *)&Default_Handler,    /* BusFault              -11 */
    (uint32_t *)&Default_Handler,    /* UsageFault            -10 */
    0, 0, 0, 0,                      /* reserved */
    (uint32_t *)&vPortSVCHandler,    /* SVCall                -5 */
    (uint32_t *)&Default_Handler,    /* DebugMon              -4 */
    0,                               /* reserved */
    (uint32_t *)&xPortPendSVHandler, /* PendSV                -2 */
    (uint32_t *)&xPortSysTickHandler,/* SysTick               -1 */
    /* External interrupts (mps2-an385): unused by this project, all default. */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void Reset_Handler(void) {
    (void)main();
    for (;;) {
        /* main() should never return; sit here if it somehow does. */
    }
}

static void Default_Handler(void) {
    for (;;) {
        /* Unhandled interrupt: park here rather than running off into the weeds. */
    }
}

void HardFault_Handler(void) {
    __asm volatile(
        " b . \n" /* park in an infinite loop on hard fault */
    );
}
