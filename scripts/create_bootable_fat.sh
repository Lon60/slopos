#!/usr/bin/env bash
set -euo pipefail

# Create a bootable FAT filesystem with embedded GRUB configuration
# This ensures GRUB automatically finds and loads the configuration

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOOT_DIR="${REPO_ROOT}/boot_fat"
KERNEL_PATH="${REPO_ROOT}/builddir/kernel.elf.elf"

info() {
  echo "[INFO] $*"
}

error() {
  echo "[ERROR] $*" >&2
  exit 1
}

# Check if kernel exists
if [ ! -f "${KERNEL_PATH}" ]; then
  error "Kernel not found at ${KERNEL_PATH}. Build the kernel first with: meson compile -C builddir"
fi

# Clean and recreate boot directory
if [ -d "${BOOT_DIR}" ]; then
  rm -rf "${BOOT_DIR}"
fi
mkdir -p "${BOOT_DIR}"/{boot,EFI/BOOT}

info "Creating bootable FAT filesystem structure..."

# Copy kernel
cp "${KERNEL_PATH}" "${BOOT_DIR}/boot/kernel.elf"

# Create embedded GRUB configuration that will be built into the EFI binary
# This config will be embedded in the GRUB EFI binary itself
cat > "${BOOT_DIR}/grub_embed.cfg" << 'EOF'
set timeout=3
set default=0

menuentry "SlopOS Kernel" {
    echo "Loading SlopOS kernel..."
    insmod fat
    insmod efi_gop
    insmod efi_uga
    insmod multiboot2

    echo "Searching for kernel on available devices..."

    # Try different device locations
    for dev in hd0 hd0,msdos1 hd1 cd0; do
        echo "Trying device: ${dev}"
        set root=(${dev})
        if test -f /boot/kernel.elf; then
            echo "Found kernel on ${dev}!"
            echo "File info:"
            file /boot/kernel.elf
            echo "Loading kernel with multiboot2 from (${dev})/boot/kernel.elf"
            multiboot2 /boot/kernel.elf
            echo "Multiboot2 load successful! Booting SlopOS..."
            boot
        fi
    done

    echo "Kernel not found on any device. Available devices:"
    ls
    echo "Falling back to manual mode..."
    halt
}

menuentry "SlopOS Debug" {
    echo "Debug mode - listing all devices and files"
    insmod fat
    insmod multiboot2

    echo "Available devices:"
    ls

    for dev in hd0 hd0,msdos1 hd1; do
        echo "Checking device: ${dev}"
        set root=(${dev})
        echo "Files on ${dev}:"
        ls /
        echo "Boot directory on ${dev}:"
        ls /boot/
        echo "---"
    done
}
EOF

# Create main GRUB configuration
cat > "${BOOT_DIR}/grub.cfg" << 'EOF'
set timeout=3
set default=0

menuentry "SlopOS Kernel" {
    echo "Loading SlopOS kernel..."
    insmod efi_gop
    insmod efi_uga
    insmod multiboot2
    multiboot2 /boot/kernel.elf
    echo "Booting SlopOS..."
    boot
}

menuentry "SlopOS Kernel (Manual)" {
    echo "Dropping to GRUB command line for manual boot..."
    echo "Use: insmod multiboot2; multiboot2 /boot/kernel.elf; boot"
    halt
}
EOF

# Check if grub-mkstandalone exists
if ! command -v grub-mkstandalone >/dev/null 2>&1; then
  error "grub-mkstandalone not found. Install grub-efi-amd64-bin package."
fi

info "Building GRUB EFI bootloader with embedded configuration..."

# Build GRUB EFI binary with embedded config that will search for grub.cfg
grub-mkstandalone \
  --format=x86_64-efi \
  --output="${BOOT_DIR}/EFI/BOOT/BOOTX64.EFI" \
  --compress=xz \
  --modules="part_gpt part_msdos fat efi_gop efi_uga multiboot2 multiboot normal boot linux configfile elf" \
  "boot/grub/grub.cfg=${BOOT_DIR}/grub_embed.cfg"

# grub.cfg is already at the root for embedded config to find

# Verify files
kernel_size=$(stat -c%s "${BOOT_DIR}/boot/kernel.elf")
bootloader_size=$(stat -c%s "${BOOT_DIR}/EFI/BOOT/BOOTX64.EFI")

info "Boot filesystem created successfully:"
info "  - Kernel: boot/kernel.elf (${kernel_size} bytes)"
info "  - Bootloader: EFI/BOOT/BOOTX64.EFI (${bootloader_size} bytes)"
info "  - Config: grub.cfg (3 second timeout, auto-boot)"
info "  - Directory: ${BOOT_DIR}"

# Clean up temporary files
rm -f "${BOOT_DIR}/grub_embed.cfg"

echo "Use: scripts/run_slopos.sh fatboot"