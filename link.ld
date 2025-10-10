OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
  /* Start kernel at 1MB where GRUB loads it */
  . = 0x100000;

  /* Multiboot2 header must come first in the file */
  .mb2 ALIGN(8) : {
    KEEP(*(.multiboot2_header))
  }

  /* Kernel start symbol */
  _kernel_start = .;

  /* All code and data in low memory for now */
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

  /* Early page tables at fixed addresses */
  . = ALIGN(0x1000);
  early_pml4 = .;
  . = . + 0x1000;  /* 4KB for PML4 */
  early_pdpt = .;
  . = . + 0x1000;  /* 4KB for PDPT */
  early_pd = .;
  . = . + 0x1000;  /* 4KB for PD */

  /* Kernel end symbol */
  _kernel_end = .;

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

