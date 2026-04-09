#!/usr/bin/env bash
set -euo pipefail

SOURCE_FILE="${1:-}"

if [[ -z "$SOURCE_FILE" ]]; then
  echo "Usage: $0 <firmware.bin>"
  exit 1
fi

if [[ ! -f "$SOURCE_FILE" ]]; then
  echo "Firmware file not found: $SOURCE_FILE"
  exit 1
fi

USER_NAME="${SUDO_USER:-$USER}"

CANDIDATES=(
  "/media/${USER_NAME}/STeaMi"
  "/run/media/${USER_NAME}/STeaMi"
  "/Volumes/STeaMi"
  "/media/${USER_NAME}/DAPLINK"
  "/run/media/${USER_NAME}/DAPLINK"
  "/Volumes/DAPLINK"
)

TARGET_DIR=""
for dir in "${CANDIDATES[@]}"; do
  if [[ -d "$dir" ]]; then
    TARGET_DIR="$dir"
    break
  fi
done

if [[ -z "$TARGET_DIR" ]]; then
  echo "STeaMi/DAPLINK drive not found. Mount the board's mass-storage volume and retry."
  echo "Looked in:"
  for dir in "${CANDIDATES[@]}"; do
    echo "  $dir"
  done
  exit 1
fi

TARGET_FILE="${TARGET_DIR}/firmware.bin"

echo "Copying ${SOURCE_FILE} -> ${TARGET_FILE}"
cp "$SOURCE_FILE" "$TARGET_FILE"
sync

echo "Upload finished via mass storage: ${TARGET_DIR}"
