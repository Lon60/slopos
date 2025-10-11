#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVMF_DIR="${REPO_ROOT}/third_party/ovmf"
OVMF_CODE="${OVMF_DIR}/OVMF_CODE.fd"
OVMF_VARS_TEMPLATE="${OVMF_DIR}/OVMF_VARS.fd"
DEFAULT_ISO="${REPO_ROOT}/slop.iso"
ISO_PATH="${1:-${DEFAULT_ISO}}"

if [ ! -f "${ISO_PATH}" ] && [ "${ISO_PATH}" = "${DEFAULT_ISO}" ]; then
  BUILD_ISO="${REPO_ROOT}/builddir/slop.iso"
  if [ -f "${BUILD_ISO}" ]; then
    ISO_PATH="${BUILD_ISO}"
  fi
fi

warn() {
  echo "$*" >&2
}

check_iso_bootability() {
  if ! command -v xorriso >/dev/null 2>&1; then
    warn "xorriso not found; skipping ISO bootability inspection"
    return 0
  fi

  local report
  if ! report="$(xorriso -indev "${ISO_PATH}" -report_el_torito plain 2>/dev/null)"; then
    warn "Failed to inspect El Torito catalog for ${ISO_PATH}"
    return 0
  fi

  local boot_line
  boot_line="$(awk '/El Torito boot img/{print}' <<<"${report}" | head -n1)"
  if [ -z "${boot_line}" ]; then
    warn "No El Torito boot image entry found in ${ISO_PATH}"
    return 0
  fi

  # Example line:
  # El Torito boot img :   1  BIOS  y   none  0x0000  0x00      4          37
  local platform
  platform="$(awk '{print $7}' <<<"${boot_line}")"

  if [ "${platform}" != "UEFI" ]; then
    warn "Detected ${platform:-unknown} El Torito platform in ${ISO_PATH} – OVMF expects a UEFI entry"
    warn "Rebuild the ISO with a UEFI boot catalog (e.g. grub-mkrescue or xorriso -eltorito-alt-boot)"
    return 1
  fi

  local path_line boot_path
  path_line="$(awk '/El Torito img path/{print}' <<<"${report}" | head -n1)"
  boot_path="$(awk '{print $7}' <<<"${path_line}")"

  if [ -z "${boot_path}" ]; then
    warn "El Torito boot path could not be determined for ${ISO_PATH}"
    return 0
  fi

  case "${boot_path}" in
    /EFI/BOOT/BOOTX64.EFI|/efiboot.img)
      echo "Verified UEFI boot catalog entry (${boot_path}) in ${ISO_PATH}"
      ;;
    *)
      warn "Unexpected UEFI boot path ${boot_path} (expected /EFI/BOOT/BOOTX64.EFI or /efiboot.img)"
      ;;
  esac
  return 0
}

cleanup_vars_copy() {
  if [ -n "${OVMF_VARS_RUNTIME:-}" ] && [ -f "${OVMF_VARS_RUNTIME}" ]; then
    rm -f "${OVMF_VARS_RUNTIME}"
  fi
}

if [ ! -f "${ISO_PATH}" ]; then
  echo "ISO image not found at ${ISO_PATH}. Build the kernel and generate slop.iso first." >&2
  exit 1
fi

if ! check_iso_bootability; then
  exit 1
fi

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
  echo "qemu-system-x86_64 not found. Install QEMU to run the kernel." >&2
  exit 1
fi

if [ ! -f "${OVMF_CODE}" ] || [ ! -f "${OVMF_VARS_TEMPLATE}" ]; then
  "${REPO_ROOT}/scripts/setup_ovmf.sh"
fi

OVMF_VARS_RUNTIME="$(mktemp "${OVMF_DIR}/OVMF_VARS.runtime.XXXXXX.fd")"
trap cleanup_vars_copy EXIT INT TERM
cp "${OVMF_VARS_TEMPLATE}" "${OVMF_VARS_RUNTIME}"

# Run the guest with serial output on stdio so Ctrl+C terminates QEMU cleanly.
# Let QEMU wire the ISO through its default AHCI controller to match OVMF's
# built-in Boot0002 DVD entry.
exec qemu-system-x86_64 \
  -machine q35,accel=tcg \
  -m 512M \
  -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
  -drive if=pflash,format=raw,file="${OVMF_VARS_RUNTIME}" \
  -device ich9-ahci,id=ahci0,bus=pcie.0,addr=0x3 \
  -drive if=none,id=cdrom,media=cdrom,readonly=on,file="${ISO_PATH}" \
  -device ide-cd,bus=ahci0.0,drive=cdrom,bootindex=0 \
  -boot order=d,menu=on \
  -serial stdio \
  -monitor none \
  -display none \
  -vga none
