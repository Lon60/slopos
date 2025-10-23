#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/builddir"
DEFAULT_KERNEL="${BUILD_DIR}/kernel.elf"
DEFAULT_OUTPUT="${BUILD_DIR}/slop.iso"
LIMINE_DIR="${REPO_ROOT}/third_party/limine"

OUTPUT_PATH="${1:-${DEFAULT_OUTPUT}}"
KERNEL_PATH="${2:-${DEFAULT_KERNEL}}"

err() {
  echo "[build_iso] $*" >&2
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    err "Missing required command '$1'. Install it (e.g. apt install $2) and retry."
    exit 1
  fi
}

require_cmd xorriso xorriso
require_cmd git git

if [ ! -f "${KERNEL_PATH}" ]; then
  err "Kernel not found at ${KERNEL_PATH}. Build the kernel first (meson compile -C builddir)."
  exit 1
fi

# Download Limine if not present
if [ ! -d "${LIMINE_DIR}" ]; then
  echo "Downloading Limine bootloader..."
  mkdir -p "${REPO_ROOT}/third_party"
  git clone https://github.com/limine-bootloader/limine.git --branch=v5.x-branch-binary --depth=1 "${LIMINE_DIR}"
fi

# Build Limine if needed
if [ ! -f "${LIMINE_DIR}/limine-bios.sys" ] || [ ! -f "${LIMINE_DIR}/BOOTX64.EFI" ]; then
  echo "Building Limine..."
  (cd "${LIMINE_DIR}" && make)
fi

# Create staging directory
STAGING_DIR="$(mktemp -d)"
trap 'rm -rf "${STAGING_DIR}"' EXIT INT TERM

ISO_ROOT="${STAGING_DIR}/iso_root"
mkdir -p "${ISO_ROOT}/boot" "${ISO_ROOT}/EFI/BOOT"

# Copy kernel
cp "${KERNEL_PATH}" "${ISO_ROOT}/boot/kernel.elf"

# Copy Limine configuration
cp "${REPO_ROOT}/limine.cfg" "${ISO_ROOT}/boot/limine.cfg"

# Copy Limine binaries
cp "${LIMINE_DIR}/limine-bios.sys" "${ISO_ROOT}/boot/"
cp "${LIMINE_DIR}/limine-bios-cd.bin" "${ISO_ROOT}/boot/"
cp "${LIMINE_DIR}/limine-uefi-cd.bin" "${ISO_ROOT}/boot/"
cp "${LIMINE_DIR}/BOOTX64.EFI" "${ISO_ROOT}/EFI/BOOT/"
cp "${LIMINE_DIR}/BOOTIA32.EFI" "${ISO_ROOT}/EFI/BOOT/" 2>/dev/null || true

# Create ISO
TMP_OUTPUT="${OUTPUT_PATH}.tmp"
trap 'rm -rf "${STAGING_DIR}" "${TMP_OUTPUT}"' EXIT INT TERM

mkdir -p "$(dirname "${OUTPUT_PATH}")"

xorriso -as mkisofs \
  -V 'SLOPOS' \
  -b boot/limine-bios-cd.bin \
  -no-emul-boot \
  -boot-load-size 4 \
  -boot-info-table \
  -eltorito-alt-boot \
  -e boot/limine-uefi-cd.bin \
  -no-emul-boot \
  -isohybrid-gpt-basdat \
  "${ISO_ROOT}" \
  -o "${TMP_OUTPUT}"

# Deploy Limine to ISO
"${LIMINE_DIR}/limine" bios-install "${TMP_OUTPUT}" 2>/dev/null || true

mv "${TMP_OUTPUT}" "${OUTPUT_PATH}"
trap - EXIT INT TERM
rm -rf "${STAGING_DIR}"

echo "Created bootable ISO (Limine) at ${OUTPUT_PATH}" >&2

