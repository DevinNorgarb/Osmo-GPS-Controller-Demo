#!/usr/bin/env bash
# Repair corrupted ESP-IDF managed_components (common after interrupted PlatformIO builds).
set -euo pipefail
cd "$(dirname "$0")/.."

IDF_PY="${IDF_PY:-$HOME/.platformio/penv/.espidf-5.5.4/bin/python}"
if [[ ! -x "$IDF_PY" ]]; then
  IDF_PY="$(command -v python3)"
fi

LVGL_DIR="components/lvgl"
LVGL_MARKER="$LVGL_DIR/src/tick/lv_tick.h"
LVGL_TAG="v9.5.0"

echo "Clearing Espressif component cache..."
"$IDF_PY" -m idf_component_manager cache clear

echo "Removing stale build artifacts..."
rm -rf managed_components managed_components/components || true
rm -rf managed_components/lvgl__lvgl || true
rm -rf components/lvgl || true
chmod -R u+w .pio 2>/dev/null || true
rm -rf .pio/build/waveshare-esp32-s3-touch-lcd-1 || true

# sdkconfig still references partitions.csv on some machines — keep a copy in sync.
cp -f partitions_waveshare.csv partitions.csv

echo "Vendoring lvgl ${LVGL_TAG} under components/lvgl (avoids truncated registry downloads)..."
mkdir -p components
TMP="$(mktemp -d)"
git clone --depth 1 --branch "$LVGL_TAG" https://github.com/lvgl/lvgl.git "$TMP"
cp -R "$TMP" "$LVGL_DIR"
rm -rf "$TMP"

if [[ ! -f "$LVGL_MARKER" ]]; then
  echo "ERROR: lvgl install failed (missing $LVGL_MARKER)" >&2
  exit 1
fi

echo "Building (single job)..."
pio run -e waveshare-esp32-s3-touch-lcd-1 -j 1

echo "Done. Flash with: pio run -e waveshare-esp32-s3-touch-lcd-1 -t upload"
