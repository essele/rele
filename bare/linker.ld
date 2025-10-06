ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x10000, LENGTH = 0x01000000  /* 16 MB RAM */
}


/* Symbols for the stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);    /* end of RAM */
_stack_size = 0x8000;                    /* 16 KB stack */
_stack_start = _estack - _stack_size;    /* stack grows downwards */

/* Compatibility symbols used by crt0/newlib */
__StackTop    = _estack;
__StackLimit  = _stack_start;
__stack       = _estack;
__stack_top   = _estack;
__stack_end__ = _stack_start;

SECTIONS
{
    .text : {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
    } > RAM

    .data : {
        __data_start__ = .;
        *(.data*)
        __data_end__ = .;
    } > RAM

    .bss : {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        __bss_end__ = .;
        __end__ = .;
        end = .;
    } > RAM

    .heap : {
        __heap_start__ = .;
        . = . + 32K; /* Heap size */
        __heap_end__ = .;
    }

    . = ALIGN(8);
    _end = .;
}
