PHDRS { text PT_LOAD FLAGS(5); }
SECTIONS {
    . = 0;
    .text   : { KEEP(*(.text.entry)) *(.text*) } :text
    .rodata : { *(.rodata*) } :text
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) *(.pdata*) *(.xdata*) }
}
