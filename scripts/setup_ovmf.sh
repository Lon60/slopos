#!/usr/bin/env bash
set -euo pipefail

# Resolve repository root relative to this script
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVMF_DIR="${REPO_ROOT}/third_party/ovmf"
OVMF_CODE="${OVMF_DIR}/OVMF_CODE.fd"
OVMF_VARS="${OVMF_DIR}/OVMF_VARS.fd"
SYSTEM_OVMF_DIR="/usr/share/OVMF"

mkdir -p "${OVMF_DIR}"

copy_system_firmware() {
  local candidate="$1"
  local dest="$2"

  if [ -f "${dest}" ]; then
    echo "OVMF artifact already present: ${dest}" >&2
    return 0
  fi

  if [ -f "${SYSTEM_OVMF_DIR}/${candidate}" ]; then
    echo "Copying ${candidate} from system OVMF install" >&2
    cp "${SYSTEM_OVMF_DIR}/${candidate}" "${dest}"
    return 0
  fi

  return 1
}

download_firmware() {
  local url="$1"
  local dest="$2"

  if [ -f "${dest}" ]; then
    echo "OVMF artifact already present: ${dest}" >&2
    return
  fi

  echo "Downloading $(basename "${dest}") from ${url}" >&2
  curl -L --fail --progress-bar "${url}" -o "${dest}"
}

# Prefer distro-provided firmware when available to avoid network downloads.
if ! copy_system_firmware "OVMF_CODE.fd" "${OVMF_CODE}" && \
   ! copy_system_firmware "OVMF_CODE_4M.fd" "${OVMF_CODE}"; then
  download_firmware "https://raw.githubusercontent.com/retrage/edk2-nightly/master/bin/RELEASEX64_OVMF_CODE.fd" "${OVMF_CODE}"
fi

if ! copy_system_firmware "OVMF_VARS.fd" "${OVMF_VARS}" && \
   ! copy_system_firmware "OVMF_VARS_4M.fd" "${OVMF_VARS}"; then
  download_firmware "https://raw.githubusercontent.com/retrage/edk2-nightly/master/bin/RELEASEX64_OVMF_VARS.fd" "${OVMF_VARS}"
fi

echo "OVMF firmware ready in ${OVMF_DIR}" >&2
