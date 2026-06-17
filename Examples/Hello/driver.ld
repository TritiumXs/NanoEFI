SECTIONS {
    . = 0;
    .text : { KEEP(*(.text.entry)) *(.text*) }
    .rodata : { *(.rodata*) }
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) *(.pdata*) *(.xdata*) }
}
