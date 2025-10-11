#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO_DIR="${REPO_ROOT}/iso"
DEFAULT_OUTPUT="${REPO_ROOT}/slop.iso"
OUTPUT_PATH="${1:-${DEFAULT_OUTPUT}}"

if ! command -v xorriso >/dev/null 2>&1; then
  echo "xorriso is required to build the UEFI ISO image." >&2
  echo "Install xorriso (e.g. apt install xorriso) and retry." >&2
  exit 1
fi

BOOTLOADER_PATH="${ISO_DIR}/EFI/BOOT/BOOTX64.EFI"
if [ ! -f "${BOOTLOADER_PATH}" ]; then
  echo "UEFI bootloader not found at ${BOOTLOADER_PATH}." >&2
  echo "Ensure grub-mkstandalone has produced BOOTX64.EFI before building the ISO." >&2
  exit 1
fi

KERNEL_PATH="${ISO_DIR}/boot/kernel.elf"
if [ ! -f "${KERNEL_PATH}" ]; then
  echo "Kernel not found at ${KERNEL_PATH}." >&2
  echo "Build the kernel (meson compile) before creating the ISO." >&2
  exit 1
fi

TMP_OUTPUT="${OUTPUT_PATH}.tmp"
trap 'rm -f "${TMP_OUTPUT}"' EXIT INT TERM
mkdir -p "$(dirname "${OUTPUT_PATH}")"

xorriso -as mkisofs \
  -V 'SLOPOS' \
  -o "${TMP_OUTPUT}" \
  -R -J \
  -eltorito-alt-boot \
  -e EFI/BOOT/BOOTX64.EFI \
  -no-emul-boot \
  -isohybrid-gpt-basdat \
  "${ISO_DIR}" >/dev/null

mv "${TMP_OUTPUT}" "${OUTPUT_PATH}"
trap - EXIT INT TERM

echo "Created UEFI bootable ISO at ${OUTPUT_PATH}" >&2
