#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out_dir="${1:-$repo_dir/build/kernel-spike}"
mkdir -p "$out_dir"

if [[ -z "${IDF_PATH:-}" ]]; then
  if [[ -f "$HOME/esp/esp-idf/tools/cmake/project.cmake" ]]; then
    export IDF_PATH="$HOME/esp/esp-idf"
  else
    echo "IDF_PATH must reference the bootstrapped ESP-IDF checkout" >&2
    exit 1
  fi
fi

if [[ -z "${IDF_PYTHON_ENV_PATH:-}" && -f "$IDF_PATH/export.sh" ]]; then
  # shellcheck disable=SC1091
  source "$IDF_PATH/export.sh" >/dev/null
fi

gcc_bin="${CROSS_COMPILE:-}riscv32-esp-elf-gcc"
if ! command -v "$gcc_bin" >/dev/null 2>&1; then
  gcc_bin="$HOME/.espressif/tools/riscv32-esp-elf/esp-15.2.0_20251204/riscv32-esp-elf/bin/riscv32-esp-elf-gcc"
fi
if [[ ! -x "$gcc_bin" ]] && ! command -v "$gcc_bin" >/dev/null 2>&1; then
  echo "riscv32-esp-elf-gcc not found" >&2
  exit 1
fi

nm_bin="${gcc_bin%gcc}nm"
elf="$out_dir/cirvane-spike.elf"
map="$out_dir/cirvane-spike.map"
bin="$out_dir/cirvane-spike.bin"

"$gcc_bin" \
  -march=rv32imc_zicsr_zifencei -mabi=ilp32 \
  -nostdlib -ffreestanding -fno-builtin -fno-pic \
  -Os -Wall -Wextra -Werror \
  -I "$repo_dir/kernel/recovery" \
  -I "$repo_dir/kernel/spike" \
  -Wl,-T,"$repo_dir/kernel/spike/linker.ld" \
  -Wl,--gc-sections \
  -Wl,-Map,"$map" \
  -o "$elf" \
  "$repo_dir/kernel/spike/start.S" \
  "$repo_dir/kernel/spike/kernel.c" \
  "$repo_dir/kernel/recovery/recovery.c"

if "${nm_bin}" "$elf" | grep -Ei 'freertos|xTaskCreate|vTaskStartScheduler|xQueueCreate'; then
  echo "spike unexpectedly contains FreeRTOS symbols" >&2
  exit 1
fi
if "${nm_bin}" "$elf" | grep -E ' [BbDd] (malloc|calloc|realloc|free)$'; then
  echo "spike unexpectedly contains allocator symbols" >&2
  exit 1
fi

esptool_py="$(command -v esptool.py || true)"
if [[ -z "$esptool_py" ]]; then
  esptool_py="$IDF_PATH/components/esptool_py/esptool/esptool.py"
fi

python3 "$esptool_py" --chip esp32c5 elf2image \
  --flash-mode dio --flash-freq 80m --flash-size 8MB \
  --output "$bin" "$elf" >/dev/null
echo "$elf"
echo "$bin"
