OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
  /* Multiboot2 header must come first in the file */
  .mb2 ALIGN(8) : {
    KEEP(*(.multiboot2_header))
  }

  /* Link kernel at higher half */
  . = 0xFFFFFFFF80000000;

  .text : {
    *(.text .text.*)
  }

  .rodata : {
    *(.rodata .rodata.*)
  }

  .data : {
    *(.data .data.*)
  }

  .bss (NOLOAD) : {
    *(.bss .bss.*)
    *(COMMON)
  }

  /DISCARD/ : {
    *(.comment*)
    *(.note*)
    *(.eh_frame*)
    *(.eh_frame_hdr)
    *(.interp)
    *(.dynamic)
    *(.dynsym)
    *(.dynstr)
    *(.gnu*)
  }
}

