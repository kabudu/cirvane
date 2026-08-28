#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"

git diff --check
forbidden_dash="$(printf '\342\200\224')"
if git grep -n "$forbidden_dash" -- .; then
  echo "Unicode U+2014 is prohibited" >&2
  exit 1
fi
python3 scripts/validate_repo.py
python3 scripts/validate_brand_candidates.py
python3 scripts/validate_brand.py
python3 tests/test_contract.py
python3 tests/test_recovery_model.py
python3 tests/test_kernel_core.py
python3 tests/test_config_rollback.py
python3 tests/test_kernel_hal.py
python3 tests/test_kernel_obs.py
python3 tests/test_kernel_stage1.py
python3 -m py_compile scripts/validate_repo.py scripts/validate_brand.py \
  scripts/validate_brand_candidates.py scripts/generate_brand_manifest.py \
  benchmarks/tools/*.py

scripts/export-brand-assets.sh build/brand-ci-a
scripts/export-brand-assets.sh build/brand-ci-b
diff \
  <(cd build/brand-ci-a && shasum -a 256 *.png) \
  <(cd build/brand-ci-b && shasum -a 256 *.png)
diff \
  <(cd assets/brand/exports && shasum -a 256 *.png) \
  <(cd build/brand-ci-a && shasum -a 256 *.png)

if [[ -z "${IDF_PATH:-}" || ! -f "${IDF_PATH}/tools/cmake/project.cmake" ]]; then
  echo "IDF_PATH must reference the bootstrapped ESP-IDF checkout" >&2
  exit 1
fi

scripts/build-kernel-spike.sh "$repo_dir/build/kernel-spike-ci" hil
scripts/build-kernel-spike.sh "$repo_dir/build/kernel-spike-ci-prod" production
scripts/build-kernel-spike.sh "$repo_dir/build/kernel-spike-ci-eval" eval

mkdir -p build/ci
if [[ ! -f build/ci/signing-key.pem ]]; then
  python -m espsecure generate-signing-key --version 2 --scheme ecdsa256 \
    build/ci/signing-key.pem
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
