#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO_TEMPLATE_DIR="${REPO_ROOT}/iso"
BUILD_DIR="${REPO_ROOT}/builddir"
DEFAULT_KERNEL="${BUILD_DIR}/kernel.elf"
DEFAULT_OUTPUT="${BUILD_DIR}/slop.iso"

OUTPUT_PATH="${1:-${DEFAULT_OUTPUT}}"
KERNEL_PATH="${2:-${DEFAULT_KERNEL}}"
GRUB_CFG_SOURCE="${ISO_TEMPLATE_DIR}/boot/grub/grub.cfg"

err() {
  echo "[build_iso] $*" >&2
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    err "Missing required command '$1'. Install it (e.g. apt install $2) and retry."
    exit 1
  fi
}

require_cmd grub-mkstandalone grub-efi-amd64-bin
require_cmd xorriso xorriso
require_cmd mkfs.vfat dosfstools
require_cmd mcopy mtools
require_cmd mmd mtools

if [ ! -f "${KERNEL_PATH}" ]; then
  err "Kernel not found at ${KERNEL_PATH}. Build the kernel first (meson compile -C builddir)."
  exit 1
fi

if [ ! -f "${GRUB_CFG_SOURCE}" ]; then
  err "GRUB configuration not found at ${GRUB_CFG_SOURCE}."
  exit 1
fi

STAGING_DIR="$(mktemp -d)"
trap 'rm -rf "${STAGING_DIR}"' EXIT INT TERM

BOOT_DIR="${STAGING_DIR}/boot"
ESP_DIR="${STAGING_DIR}/EFI/BOOT"
mkdir -p "${BOOT_DIR}/grub" "${ESP_DIR}"

cp "${KERNEL_PATH}" "${BOOT_DIR}/kernel.elf"
cp "${GRUB_CFG_SOURCE}" "${BOOT_DIR}/grub/grub.cfg"

EMBED_CFG="$(mktemp)"
trap 'rm -rf "${STAGING_DIR}" "${EMBED_CFG}"' EXIT INT TERM
cat > "${EMBED_CFG}" <<'CFG'
search --no-floppy --file --set=bootdev /boot/kernel.elf
set root=($bootdev)
set prefix=($root)/boot/grub

if test -f ($prefix)/grub.cfg; then
  configfile ($prefix)/grub.cfg
else
  echo "Booting SlopOS kernel directly from ${root}"
  multiboot2 /boot/kernel.elf
  boot
fi
CFG

grub-mkstandalone \
  -O x86_64-efi \
  --locales= \
  --fonts= \
  --modules="efi_gop efi_uga multiboot2 normal configfile search search_fs_file iso9660 part_gpt test echo" \
  -o "${ESP_DIR}/BOOTX64.EFI" \
  "boot/grub/grub.cfg=${EMBED_CFG}" >/dev/null

ESP_IMAGE="${STAGING_DIR}/efiboot.img"
truncate -s 24M "${ESP_IMAGE}"
mkfs.vfat -n SLOPOS_EFI "${ESP_IMAGE}" >/dev/null
mmd -i "${ESP_IMAGE}" ::/EFI ::/EFI/BOOT >/dev/null
mcopy -i "${ESP_IMAGE}" "${ESP_DIR}/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI >/dev/null
mmd -i "${ESP_IMAGE}" ::/boot ::/boot/grub >/dev/null
mcopy -i "${ESP_IMAGE}" "${BOOT_DIR}/kernel.elf" ::/boot/kernel.elf >/dev/null
mcopy -i "${ESP_IMAGE}" "${BOOT_DIR}/grub/grub.cfg" ::/boot/grub/grub.cfg >/dev/null

TMP_OUTPUT="${OUTPUT_PATH}.tmp"
trap 'rm -rf "${STAGING_DIR}" "${EMBED_CFG}" "${TMP_OUTPUT}" "${ESP_IMAGE}"' EXIT INT TERM
mkdir -p "$(dirname "${OUTPUT_PATH}")"

xorriso -as mkisofs \
  -V 'SLOPOS' \
  -o "${TMP_OUTPUT}" \
  -R -J \
  -eltorito-alt-boot \
  -e efiboot.img \
  -no-emul-boot \
  -isohybrid-gpt-basdat \
  -append_partition 2 0xef "${ESP_IMAGE}" \
  "${STAGING_DIR}" >/dev/null

mv "${TMP_OUTPUT}" "${OUTPUT_PATH}"
trap - EXIT INT TERM
rm -rf "${STAGING_DIR}" "${EMBED_CFG}" "${ESP_IMAGE}"

echo "Created UEFI bootable ISO at ${OUTPUT_PATH}" >&2
