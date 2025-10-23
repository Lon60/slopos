#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVMF_DIR="${REPO_ROOT}/third_party/ovmf"
OVMF_CODE="${OVMF_DIR}/OVMF_CODE.fd"
OVMF_VARS_TEMPLATE="${OVMF_DIR}/OVMF_VARS.fd"
DEFAULT_ISO="${REPO_ROOT}/slop.iso"
# First arg is ISO path (same as run_qemu_ovmf.sh)
ISO_PATH="${1:-${DEFAULT_ISO}}"
# Optional second arg selects display backend: gtk or sdl (defaults to gtk)
DISPLAY_BACKEND="${2:-gtk}"

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

  local boot_lines
  boot_lines="$(awk '/El Torito boot img/{print}' <<<"${report}")"
  if [ -z "${boot_lines}" ]; then
    warn "No El Torito boot image entries found in ${ISO_PATH}"
    return 0
  fi

  local has_uefi
  has_uefi="$(awk '{print $7}' <<<"${boot_lines}" | grep -c "UEFI" || true)"

  if [ "${has_uefi}" -eq 0 ]; then
    warn "No UEFI El Torito entry found in ${ISO_PATH} – OVMF requires UEFI boot support"
    warn "Rebuild the ISO with a UEFI boot catalog (e.g. xorriso -eltorito-alt-boot)"
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
    /boot/limine-bios-cd.bin|/boot/limine-uefi-cd.bin)
      echo "Verified Limine UEFI boot catalog entry (${boot_path}) in ${ISO_PATH}"
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

# Detect available display backends
detect_display_backend() {
  local available_displays
  available_displays="$(qemu-system-x86_64 -display help 2>&1 | grep -v "^Available" | grep -v "^Some" | grep -v "^For" | grep -v "^$" | awk '{print $1}' | grep -vw "none")"
  
  # If no displays available at all
  if [ -z "${available_displays}" ]; then
    return 1
  fi
  
  # Try user's preference first (but not 'none')
  if [ "${DISPLAY_BACKEND}" != "none" ] && echo "${available_displays}" | grep -qw "${DISPLAY_BACKEND}"; then
    echo "${DISPLAY_BACKEND}"
    return 0
  fi
  
  # Fall back to common displays in order of preference
  for display in gtk sdl cocoa vnc curses; do
    if echo "${available_displays}" | grep -qw "${display}"; then
      warn "Display backend '${DISPLAY_BACKEND}' not available, using '${display}'"
      echo "${display}"
      return 0
    fi
  done
  
  # No graphical display available
  return 1
}

DETECTED_DISPLAY="$(detect_display_backend || echo "none")"

if [ "${DETECTED_DISPLAY}" = "none" ]; then
  echo "ERROR: No graphical display backends available in QEMU." >&2
  echo "Your QEMU installation was compiled without display support." >&2
  echo "" >&2
  echo "To fix this, install QEMU with graphical support:" >&2
  echo "  • Arch/CachyOS: pacman -S qemu-ui-gtk qemu-ui-sdl" >&2
  echo "  • Ubuntu/Debian: apt install qemu-system-gui" >&2
  echo "  • Fedora: dnf install qemu-ui-gtk qemu-ui-sdl" >&2
  echo "" >&2
  echo "Alternatively, use VNC by installing qemu-ui-vnc and connecting with a VNC viewer." >&2
  exit 1
fi

# Special handling for VNC
DISPLAY_ARGS="-display ${DETECTED_DISPLAY}"
if [ "${DETECTED_DISPLAY}" = "vnc" ]; then
  DISPLAY_ARGS="-vnc :0"
  echo "Starting QEMU with VNC server on localhost:5900"
  echo "Connect with: vncviewer localhost:5900"
fi

# Run the guest with a graphical display. Keep serial on stdio so kernel output still appears.
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
  ${DISPLAY_ARGS} \
  -vga std
