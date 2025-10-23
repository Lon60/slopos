OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

/* Limine loads kernel at higher half virtual address */
/* Physical memory is identity-mapped by Limine */
KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;

PHDRS
{
  text PT_LOAD FLAGS(5);  /* R-X (read + execute) */
  rodata PT_LOAD FLAGS(4);  /* R-- (read only) */
  data PT_LOAD FLAGS(6);  /* RW- (read + write) */
}

SECTIONS
{
  /* Kernel starts at higher half virtual address */
  . = KERNEL_VIRT_BASE;

  /* Limine requests section - must be in first LOAD segment */
  .limine_requests ALIGN(4096) : {
    KEEP(*(.limine_requests_start_marker))
    KEEP(*(.limine_requests))
    KEEP(*(.limine_requests_end_marker))
  } :text

  /* Code section */
  .text ALIGN(4096) : {
    *(.text .text.*)
  } :text

  /* Kernel start symbol */
  _kernel_start = .;

  .rodata ALIGN(4096) : {
    *(.rodata .rodata.*)
  } :rodata

  .data ALIGN(4096) : {
    *(.data .data.*)
  } :data

  .bss (NOLOAD) : ALIGN(4096) {
    *(.bss .bss.*)
    *(COMMON)
  } :data

  /* Early page tables at fixed addresses */
  . = ALIGN(0x1000);
  early_pml4 = .;
  . = . + 0x1000;  /* 4KB for PML4 */
  early_pdpt = .;
  . = . + 0x1000;  /* 4KB for PDPT */
  early_pd_identity = .;
  . = . + 0x1000;  /* 4KB for identity mapping PD */
  early_pd_kernel = .;
  . = . + 0x1000;  /* 4KB for higher-half kernel PD */

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

