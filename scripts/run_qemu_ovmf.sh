#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVMF_DIR="${REPO_ROOT}/third_party/ovmf"
OVMF_CODE="${OVMF_DIR}/OVMF_CODE.fd"
OVMF_VARS_TEMPLATE="${OVMF_DIR}/OVMF_VARS.fd"
OVMF_VARS_RUNTIME="${OVMF_DIR}/OVMF_VARS.runtime.fd"
ISO_PATH="${1:-${REPO_ROOT}/slop.iso}"

if [ ! -f "${ISO_PATH}" ]; then
  echo "ISO image not found at ${ISO_PATH}. Build the kernel and generate slop.iso first." >&2
  exit 1
fi

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
  echo "qemu-system-x86_64 not found. Install QEMU to run the kernel." >&2
  exit 1
fi

if [ ! -f "${OVMF_CODE}" ] || [ ! -f "${OVMF_VARS_TEMPLATE}" ]; then
  "${REPO_ROOT}/scripts/setup_ovmf.sh"
fi

cp "${OVMF_VARS_TEMPLATE}" "${OVMF_VARS_RUNTIME}"

exec qemu-system-x86_64 \
  -machine q35,accel=tcg \
  -m 512M \
  -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
  -drive if=pflash,format=raw,file="${OVMF_VARS_RUNTIME}" \
  -cdrom "${ISO_PATH}" \
  -serial stdio \
  -monitor none
