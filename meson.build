project('slopos', 'c',
  version : '0.1.0',
  default_options : [
    'c_std=c11',
    'warning_level=3',
    'werror=false'
  ]
)

# Source files organized by component
boot_sources = []
mm_sources = []
video_sources = []
drivers_sources = []
kernel_sources = []

# Boot directory - assembly and early C code
boot_dir = meson.current_source_dir() / 'boot'
boot_sources += files(
  'boot/multiboot2_header.s',
  'boot/entry32.s',
  'boot/gdt_setup.s',
  'boot/entry64.s',
  'boot/early_init.c',
  'boot/cpu_verify.c',
  'boot/kernel_panic.c'
)

# Memory management directory
mm_dir = meson.current_source_dir() / 'mm'
mm_sources += files(
  'mm/multiboot2.c',
  'mm/paging.c',
  'mm/memory_layout.c',
  'mm/mb2_parser.c',
  'mm/page_alloc.c',
  'mm/process_vm.c',
  'mm/kernel_heap.c',
  'mm/early_paging.c',
  'mm/uefi_memory.c',
  'mm/buddy_alloc.c',
  'mm/vmem_regions.c'
)

# Video/framebuffer directory
video_dir = meson.current_source_dir() / 'video'
video_sources += files()  # Will be populated when source files exist

# Drivers directory
drivers_dir = meson.current_source_dir() / 'drivers'
drivers_sources += files(
  'drivers/serial.c'
)

# Kernel main sources (root directory)
kernel_sources += files()  # Will be populated when source files exist

# Combine all source files
all_sources = boot_sources + mm_sources + video_sources + drivers_sources + kernel_sources

# Include directories
inc_dirs = include_directories('.', 'boot', 'mm', 'video', 'drivers')

# Custom target for assembly files (when they exist)
# This will be used for boot assembly code that needs special handling

# Main kernel executable
kernel_elf = executable('kernel.elf',
  sources : all_sources,
  include_directories : inc_dirs,
  link_args : [
    '-T', meson.current_source_dir() / 'link.ld'
  ],
  link_depends : files('link.ld'),
  install : false,
  name_suffix : 'elf'
)

# Build summary
summary({
  'Target': 'x86_64-unknown-none',
  'Boot Protocol': 'Multiboot2',
  'Kernel Type': 'Freestanding Higher-Half',
  'Linker Script': 'link.ld',
  'Output': 'kernel.elf'
}, section: 'SlopOS Kernel Build')

# Custom target to copy kernel.elf to iso/boot/ for GRUB
kernel_iso_copy = custom_target('kernel_iso_copy',
  input : kernel_elf,
  output : 'kernel.elf.copied',
  command : ['cp', '@INPUT@', meson.current_source_dir() / 'iso' / 'boot' / 'kernel.elf'],
  build_by_default : true,
  depends : kernel_elf
)