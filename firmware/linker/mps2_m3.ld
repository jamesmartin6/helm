/*
 * Linker script for the Helm firmware targeting QEMU's mps2-an385 machine
 * (Cortex-M3, ARM AN385 memory map). Adapted from FreeRTOS's own
 * CORTEX_MPS2_QEMU_IAR_GCC demo linker script.
 *
 * QEMU's `-kernel` ELF loader writes each PT_LOAD segment directly to its
 * target physical address, including RAM -- so unlike real hardware, no
 * .data copy-from-flash step is needed at startup; the .data section's
 * *initial values* are simply loaded straight into RAM at boot.
 */

MEMORY
{
    FLASH (xr) : ORIGIN = 0x00000000, LENGTH = 4M
    RAM   (rw) : ORIGIN = 0x20000000, LENGTH = 4M
}

ENTRY(Reset_Handler)

_Min_Heap_Size = 0x8;      /* Not used -- FreeRTOS heap_4.c manages its own heap region. */
_Min_Stack_Size = 0x1000;  /* Startup/ISR stack, reused as interrupt stack after scheduler starts. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector :
    {
        __vector_table = .;
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        _etext = .;
        _sidata = .;
    } > FLASH

    .data :
    {
        . = ALIGN(8);
        _sdata = .;
        *(vtable)
        *(.data*)
        _edata = .;
    } > RAM

    .bss :
    {
        . = ALIGN(8);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > RAM

    .heap :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        _heap_bottom = .;
        . = . + _Min_Heap_Size;
        _heap_top = .;
    } > RAM

    __StackTop = ORIGIN(RAM) + LENGTH(RAM);
    __StackLimit = __StackTop - _Min_Stack_Size;
    PROVIDE(__stack = __StackTop);

    ASSERT(__StackLimit >= _heap_top, "region RAM overflowed: stack + heap + data + bss exceed RAM size")
}
