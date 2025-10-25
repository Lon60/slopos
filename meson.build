project('slopos', 'c',
  version : '0.1.0',
  default_options : [
    'c_std=c11',
    'warning_level=3',
    'werror=false',
    'b_staticpic=false',
    'b_lundef=false',
    'buildtype=debug'
  ]
)

add_project_arguments([
    '-ffreestanding',
    '-fno-stack-protector',
    '-fno-pic',
    '-mno-red-zone',
    '-mcmodel=kernel',
    '-mabi=sysv',
    # Disable SIMD/vector instructions to avoid "Invalid Opcode" exceptions
    # These instructions may not be supported by QEMU or cause issues with large structures
    '-mno-sse',
    '-mno-sse2',
    '-mno-mmx',
    '-mno-avx',
    '-mno-avx2',
    '-msoft-float',
    '-Wall',
    '-Wextra',
    '-Wno-unused-parameter'
  ],
  language: 'c')

# Source files organized by component
boot_sources = []
mm_sources = []
video_sources = []
drivers_sources = []
sched_sources = []
kernel_sources = []
lib_sources = []

# Boot directory - assembly and early C code
boot_dir = meson.current_source_dir() / 'boot'
boot_sources += files(
  'boot/limine_entry.s',
  'boot/idt_handlers.s',
  'boot/early_init.c',
  'boot/cpu_verify.c',
  'boot/kernel_panic.c',
  'boot/idt.c',
  'boot/debug.c',
  'boot/shutdown.c',
  'boot/limine_protocol.c'
)

# Memory management directory
mm_dir = meson.current_source_dir() / 'mm'
mm_sources += files(
  'mm/paging.c',
  'mm/memory_layout.c',
  'mm/mb2_parser.c',
  'mm/page_alloc.c',
  'mm/process_vm.c',
  'mm/kernel_heap.c',
  'mm/early_paging.c',
  'mm/uefi_memory.c',
  'mm/buddy_alloc.c',
  'mm/vmem_regions.c',
  'mm/memory_init.c',
  'mm/phys_virt.c'
)

# Video/framebuffer directory
video_dir = meson.current_source_dir() / 'video'
video_sources += files(
  'video/framebuffer.c',
  'video/graphics.c',
  'video/font.c'
)

# Lib directory - utility functions
lib_dir = meson.current_source_dir() / 'lib'
lib_sources += files(
  'lib/string.c'
)

# Drivers directory
drivers_dir = meson.current_source_dir() / 'drivers'
drivers_sources += files(
  'drivers/serial.c',
  'drivers/pic.c',
  'drivers/apic.c',
  'drivers/interrupt_test.c',
  'drivers/exceptions.s',
  'drivers/exception_handlers.c'
)

# Scheduler directory
sched_dir = meson.current_source_dir() / 'sched'
sched_sources += files(
  'sched/scheduler.c',
  'sched/task.c',
  'sched/test_tasks.c',
  'sched/context_switch.s'
)

# Kernel main sources (root directory)
kernel_sources += files()  # Will be populated when source files exist

# Combine all source files
all_sources = boot_sources + mm_sources + video_sources + drivers_sources + sched_sources + kernel_sources + lib_sources

# Include directories
inc_dirs = include_directories('.', 'boot', 'mm', 'video', 'drivers', 'sched', 'lib')

# Custom target for assembly files (when they exist)
# This will be used for boot assembly code that needs special handling

# Main kernel executable
linker_script = meson.current_source_dir() / 'link.ld'

kernel_elf = executable('kernel',
  sources : all_sources,
  include_directories : inc_dirs,
  c_args : [
    '-O2',
    '-fno-builtin',
    '-mcmodel=kernel',
    '-fno-PIE'
  ],
  link_args : [
    '-nostdlib',
    '-static',
    '-Wl,-T,' + linker_script,
    '-Wl,-z,max-page-size=0x1000',
    '-Wl,-z,common-page-size=0x1000',
    '-Wl,--build-id=none',
    '-lgcc'
  ],
  link_depends : files('link.ld'),
  install : false,
  name_suffix : 'elf'
)

# Build summary
summary({
  'Target': 'x86_64-unknown-none',
  'Boot Protocol': 'Limine Native',
  'Kernel Type': 'Freestanding Higher-Half',
  'Linker Script': 'link.ld',
  'Output': 'kernel.elf'
}, section: 'SlopOS Kernel Build')
