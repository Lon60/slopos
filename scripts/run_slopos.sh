#!/usr/bin/env bash
set -euo pipefail

# SlopOS Boot Script - Reliable UEFI Boot with OVMF
# This script provides multiple boot methods: FAT filesystem (primary) and ISO fallback

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVMF_DIR="${REPO_ROOT}/third_party/ovmf"
OVMF_CODE="${OVMF_DIR}/OVMF_CODE.fd"
OVMF_VARS_TEMPLATE="${OVMF_DIR}/OVMF_VARS.fd"
TEST_EFI_DIR="${REPO_ROOT}/test_efi_dir"
BOOT_FAT_DIR="${REPO_ROOT}/boot_fat"
ISO_PATH="${REPO_ROOT}/slop.iso"

# Boot method: fat (default), fatboot, or iso
BOOT_METHOD="${1:-fat}"
TIMEOUT="${2:-30}"

warn() {
  echo "[WARNING] $*" >&2
}

error() {
  echo "[ERROR] $*" >&2
  exit 1
}

info() {
  echo "[INFO] $*"
}

cleanup_vars_copy() {
  if [ -n "${OVMF_VARS_RUNTIME:-}" ] && [ -f "${OVMF_VARS_RUNTIME}" ]; then
    rm -f "${OVMF_VARS_RUNTIME}"
  fi
}

check_dependencies() {
  if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    error "qemu-system-x86_64 not found. Install QEMU to run the kernel."
  fi

  if [ ! -f "${OVMF_CODE}" ] || [ ! -f "${OVMF_VARS_TEMPLATE}" ]; then
    info "Setting up OVMF firmware..."
    "${REPO_ROOT}/scripts/setup_ovmf.sh"
  fi
}

verify_kernel() {
  local kernel_path="$1"
  if [ ! -f "${kernel_path}" ]; then
    error "Kernel not found at ${kernel_path}. Build the kernel first."
  fi

  local size=$(stat -c%s "${kernel_path}")
  if [ "${size}" -lt 1024 ]; then
    warn "Kernel file is very small (${size} bytes). May not be properly built."
  fi

  info "Kernel verified: $(basename "${kernel_path}") (${size} bytes)"
}

boot_fat_method() {
  info "Using FAT filesystem boot method..."

  # Verify test_efi_dir structure
  if [ ! -d "${TEST_EFI_DIR}" ]; then
    error "test_efi_dir not found. Kernel build may not have completed properly."
  fi

  verify_kernel "${TEST_EFI_DIR}/boot/kernel.elf"

  if [ ! -f "${TEST_EFI_DIR}/EFI/BOOT/BOOTX64.EFI" ]; then
    error "GRUB EFI bootloader not found in test_efi_dir/EFI/BOOT/BOOTX64.EFI"
  fi

  info "FAT filesystem structure verified"

  # Create runtime OVMF vars copy
  OVMF_VARS_RUNTIME="$(mktemp "${OVMF_DIR}/OVMF_VARS.runtime.XXXXXX.fd")"
  trap cleanup_vars_copy EXIT INT TERM
  cp "${OVMF_VARS_TEMPLATE}" "${OVMF_VARS_RUNTIME}"

  info "Starting QEMU with FAT filesystem boot (timeout: ${TIMEOUT}s)..."
  info "GRUB should auto-boot kernel in 5 seconds, or you can:"
  info "  - Wait for auto-boot"
  info "  - Press Enter to select menu item"
  info "  - Type 'c' for GRUB command line, then:"
  info "    grub> insmod multiboot2"
  info "    grub> multiboot2 /boot/kernel.elf"
  info "    grub> boot"
  info "Expected final output: 'SlopOS Booted!' message"
  info "Press Ctrl+C to exit"
  echo

  # Create a temporary script to send auto-boot commands if needed
  QEMU_SCRIPT=$(mktemp)
  cat > "${QEMU_SCRIPT}" << 'EOF'
#!/bin/bash
# Wait a moment then send enter to trigger menu selection
sleep 6
# If GRUB menu doesn't auto-start, we can send these commands:
# echo -e "\x63" > /proc/self/fd/1  # 'c' for command line
# sleep 1
# echo -e "insmod multiboot2\r" > /proc/self/fd/1
# sleep 1
# echo -e "multiboot2 /boot/kernel.elf\r" > /proc/self/fd/1
# sleep 1
# echo -e "boot\r" > /proc/self/fd/1
EOF
  chmod +x "${QEMU_SCRIPT}"

  # Use timeout to prevent hanging
  timeout "${TIMEOUT}" qemu-system-x86_64 \
    -machine q35,accel=tcg \
    -m 512M \
    -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
    -drive if=pflash,format=raw,file="${OVMF_VARS_RUNTIME}" \
    -drive file="fat:rw:${TEST_EFI_DIR}",format=raw \
    -boot order=c,menu=on \
    -serial stdio \
    -monitor none \
    -display none \
    -vga none \
    || {
      local exit_code=$?
      if [ ${exit_code} -eq 124 ]; then
        echo
        info "QEMU timeout reached (${TIMEOUT}s). This is normal for testing."
      else
        echo
        warn "QEMU exited with code ${exit_code}"
      fi
    }

  rm -f "${QEMU_SCRIPT}"
}

boot_fatboot_method() {
  info "Using FAT filesystem boot method with embedded GRUB config..."

  # Check if boot_fat directory exists
  if [ ! -d "${BOOT_FAT_DIR}" ]; then
    info "boot_fat directory not found. Creating it now..."
    "${REPO_ROOT}/scripts/create_bootable_fat.sh"
  fi

  verify_kernel "${BOOT_FAT_DIR}/boot/kernel.elf"

  if [ ! -f "${BOOT_FAT_DIR}/EFI/BOOT/BOOTX64.EFI" ]; then
    error "GRUB EFI bootloader not found in boot_fat/EFI/BOOT/BOOTX64.EFI"
  fi

  if [ ! -f "${BOOT_FAT_DIR}/grub.cfg" ]; then
    error "GRUB configuration not found in boot_fat/grub.cfg"
  fi

  info "FAT boot filesystem structure verified"

  # Create runtime OVMF vars copy
  OVMF_VARS_RUNTIME="$(mktemp "${OVMF_DIR}/OVMF_VARS.runtime.XXXXXX.fd")"
  trap cleanup_vars_copy EXIT INT TERM
  cp "${OVMF_VARS_TEMPLATE}" "${OVMF_VARS_RUNTIME}"

  info "Starting QEMU with FAT boot filesystem (timeout: ${TIMEOUT}s)..."
  info "GRUB should auto-boot kernel in 3 seconds with menu"
  info "Expected output: GRUB menu -> auto-boot -> 'SlopOS Booted!'"
  info "Press Ctrl+C to exit"
  echo

  # Use timeout to prevent hanging
  timeout "${TIMEOUT}" qemu-system-x86_64 \
    -machine q35,accel=tcg \
    -m 512M \
    -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
    -drive if=pflash,format=raw,file="${OVMF_VARS_RUNTIME}" \
    -drive file="fat:rw:${BOOT_FAT_DIR}",format=raw \
    -boot order=c,menu=on \
    -serial stdio \
    -monitor none \
    -display none \
    -vga none \
    || {
      local exit_code=$?
      if [ ${exit_code} -eq 124 ]; then
        echo
        info "QEMU timeout reached (${TIMEOUT}s). This is normal for testing."
      else
        echo
        warn "QEMU exited with code ${exit_code}"
      fi
    }
}

boot_iso_method() {
  info "Using ISO boot method..."

  if [ ! -f "${ISO_PATH}" ]; then
    error "ISO image not found at ${ISO_PATH}. Build the ISO first."
  fi

  local iso_size=$(stat -c%s "${ISO_PATH}")
  info "ISO verified: $(basename "${ISO_PATH}") (${iso_size} bytes)"

  # Create runtime OVMF vars copy
  OVMF_VARS_RUNTIME="$(mktemp "${OVMF_DIR}/OVMF_VARS.runtime.XXXXXX.fd")"
  trap cleanup_vars_copy EXIT INT TERM
  cp "${OVMF_VARS_TEMPLATE}" "${OVMF_VARS_RUNTIME}"

  info "Starting QEMU with ISO boot (timeout: ${TIMEOUT}s)..."
  info "Note: ISO boot may have issues with UEFI boot catalog"
  info "Press Ctrl+C to exit"
  echo

  # Use timeout to prevent hanging
  timeout "${TIMEOUT}" qemu-system-x86_64 \
    -machine q35,accel=tcg \
    -m 512M \
    -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
    -drive if=pflash,format=raw,file="${OVMF_VARS_RUNTIME}" \
    -cdrom "${ISO_PATH}" \
    -boot order=d,menu=on \
    -serial stdio \
    -monitor none \
    -display none \
    -vga none \
    || {
      local exit_code=$?
      if [ ${exit_code} -eq 124 ]; then
        echo
        info "QEMU timeout reached (${TIMEOUT}s). This is normal for testing."
      else
        echo
        warn "QEMU exited with code ${exit_code}"
      fi
    }
}

show_usage() {
  cat << EOF
SlopOS Boot Script

Usage: $0 [METHOD] [TIMEOUT]

Methods:
  fat     - Use FAT filesystem boot (uses test_efi_dir)
  fatboot - Use FAT filesystem boot with embedded GRUB config (recommended)
  iso     - Use ISO CD-ROM boot (may have UEFI issues)

Timeout:
  Number of seconds to run QEMU before auto-exit (default: 30)

Examples:
  $0                  # FAT boot with 30s timeout
  $0 fatboot          # FAT boot with embedded config (recommended)
  $0 fatboot 60       # FAT boot with 60s timeout
  $0 iso              # ISO boot with 30s timeout

Expected output on successful boot:
  - GRUB menu appears
  - Kernel loads with Multiboot2
  - Serial console shows "SlopOS Booted!" message
EOF
}

# Main execution
case "${BOOT_METHOD}" in
  fat)
    check_dependencies
    boot_fat_method
    ;;
  fatboot)
    check_dependencies
    boot_fatboot_method
    ;;
  iso)
    check_dependencies
    boot_iso_method
    ;;
  help|--help|-h)
    show_usage
    exit 0
    ;;
  *)
    error "Unknown boot method '${BOOT_METHOD}'. Use 'fat', 'fatboot', 'iso', or 'help'."
    ;;
esac