#!/usr/bin/env bash
set -euo pipefail

# Resolve repository root relative to this script
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVMF_DIR="${REPO_ROOT}/third_party/ovmf"
OVMF_CODE="${OVMF_DIR}/OVMF_CODE.fd"
OVMF_VARS="${OVMF_DIR}/OVMF_VARS.fd"

mkdir -p "${OVMF_DIR}"

fetch() {
  local url="$1"
  local dest="$2"
  if [ -f "${dest}" ]; then
    echo "OVMF artifact already present: ${dest}" >&2
    return
  fi

  echo "Downloading $(basename "${dest}") from ${url}" >&2
  curl -L --fail --progress-bar "${url}" -o "${dest}"
}

fetch "https://raw.githubusercontent.com/retrage/edk2-nightly/master/bin/RELEASEX64_OVMF_CODE.fd" "${OVMF_CODE}"
fetch "https://raw.githubusercontent.com/retrage/edk2-nightly/master/bin/RELEASEX64_OVMF_VARS.fd" "${OVMF_VARS}"

echo "OVMF firmware downloaded to ${OVMF_DIR}" >&2
