#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"

git diff --check
if git grep -n $'\u2014' -- .; then
  echo "Unicode U+2014 is prohibited" >&2
  exit 1
fi
python3 scripts/validate_repo.py
python3 tests/test_contract.py
python3 -m py_compile scripts/validate_repo.py benchmarks/tools/*.py

if [[ -z "${IDF_PATH:-}" || ! -f "${IDF_PATH}/tools/cmake/project.cmake" ]]; then
  echo "IDF_PATH must reference the bootstrapped ESP-IDF checkout" >&2
  exit 1
fi

mkdir -p build/ci
if [[ ! -f build/ci/signing-key.pem ]]; then
  idf.py secure-generate-signing-key --scheme ecdsa256 build/ci/signing-key.pem
fi

idf.py -B build/ci \
  -D SDKCONFIG="$repo_dir/build/ci/sdkconfig" \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ci.defaults' build

if grep -q '^CONFIG_NUCLEUS_HIL_DIAGNOSTICS=y$' build/ci/sdkconfig; then
  echo "production CI accidentally enabled HIL diagnostics" >&2
  exit 1
fi
if strings build/ci/nucleus.bin | grep -Eq 'ota-reject-corrupt|config-corrupt-test|ota-stage-self|svcfail'; then
  echo "production image exposes a HIL-only command" >&2
  exit 1
fi

echo "Cirvane local CI passed"
