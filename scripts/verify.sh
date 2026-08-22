#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
REGENERATE=0

if [[ "${1:-}" == "--regenerate" ]]; then
  REGENERATE=1
elif [[ $# -ne 0 ]]; then
  echo "usage: $0 [--regenerate]" >&2
  exit 2
fi

command -v gcc >/dev/null
command -v python3 >/dev/null
command -v sha256sum >/dev/null

cd "$ROOT"
sha256sum --check --strict SHA256SUMS.txt

rm -rf "$BUILD"
mkdir -p "$BUILD/final-check"

gcc -O2 -std=c99 -Wall -Wextra -o "$BUILD/certify" src/certify.c
"$BUILD/certify" artifacts/safe6.bin | tee "$BUILD/certify-safe6.txt"
"$BUILD/certify" artifacts/sigma.bin | tee "$BUILD/certify-sigma.txt"

for file in engine.py test_engine.py dualbook.py verify_dual.py; do
  cp "src/python/$file" "$BUILD/final-check/$file"
done
for file in allowed2.bin book_dual.pkl values.bin; do
  cp "artifacts/$file" "$BUILD/final-check/$file"
done

(
  cd "$BUILD/final-check"
  PYTHONUTF8=1 python3 test_engine.py
  PYTHONUTF8=1 python3 verify_dual.py
)

if [[ "$REGENERATE" -eq 1 ]]; then
  mkdir -p "$BUILD/regenerate"
  cp src/safety6.c src/solve.c "$BUILD/regenerate/"
  cp src/python/engine.py src/python/prune3.py "$BUILD/regenerate/"

  (
    cd "$BUILD/regenerate"
    gcc -O2 -std=c99 -Wall -Wextra -o safety6 safety6.c
    FRESH=1 PYTHONUTF8=1 python3 prune3.py
    cmp --silent safe6.bin "$ROOT/artifacts/safe6.bin"
    cmp --silent allowed2.bin "$ROOT/artifacts/allowed2.bin"

    gcc -O2 -std=c99 -Wall -Wextra -o solve solve.c
    ./solve solve
    cmp --silent values.bin "$ROOT/artifacts/values.bin"
  )
  echo "DETERMINISTIC ARTIFACT REGENERATION PASSED"
fi

echo "PROOF ARTIFACT VERIFICATION PASSED"
