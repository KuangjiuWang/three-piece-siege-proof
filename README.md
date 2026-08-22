# Three-Piece Siege: Proof Artifact

This repository is the reproducibility artifact for the certificate-based non-loss proof
in *Three-Piece Siege on the 4x4 Board is a Draw*. It contains the final certificates,
their independent local verifier, the executable rule book, the reference rules engine,
and the LaTeX source of the accompanying paper.

## Claim and scope

The main claim is that, under the stated history-sensitive threefold-repetition rule, each
player has a positional strategy that guarantees at least a draw from the initial position.
The proof uses local safety certificates. It does **not** claim new general safety-game
theory, and `values.bin` is only an auxiliary history-free classification: it is not an
exact solution of the history-sensitive game.

## Certificates

The position universe contains all legal triples `(B, W, t)` with three or four black men,
three or four white men, and side to move `t`. Each certificate is an 11,328,800-byte
membership vector indexed by

```text
sidx(B, W, t) = (rank(B) * 2380 + rank(W)) * 2 + t
```

where `rank` enumerates 16-bit masks of Hamming weight three or four in increasing order.
A byte value of `1` denotes membership.

- `artifacts/safe6.bin` is the main two-colour safety certificate. `src/certify.c` checks
  legality, Black safety closure, initial-state membership, and White safety closure after
  colour-reversing rotation.
- `artifacts/sigma.bin` is the smaller reachable certificate induced when Black follows
  the executable rule book. It is checked by the same verifier.
- `artifacts/book_dual.pkl` and `artifacts/allowed2.bin` encode the rule book and its
  permitted Black shapes. `src/python/verify_dual.py` exhaustively expands every opposing
  action for both colour-conjugate uses of that book.
- `artifacts/values.bin` is used only for an auxiliary cross-check of the rule book. It
  must not be read as an exact value table for the history-sensitive game.

## Reproduce

On a Linux system with Bash, Python 3, GNU coreutils, and a C99 compiler:

```bash
bash scripts/verify.sh
```

This verifies all SHA-256 digests, builds the local C verifier, checks both certificates,
runs the reference-rule tests, and exhaustively checks the dual rule book. The expected
success conditions are recorded in `expected-output.txt`.

To additionally regenerate the two deterministic artifacts that have a complete source
pipeline in this repository:

```bash
bash scripts/verify.sh --regenerate
```

That mode reconstructs `safe6.bin`, `allowed2.bin`, and `values.bin` in a temporary build
directory, then compares their bytes with the published artifacts. It does not regenerate
the compact rule book; the rule book is a separately published, hash-checked certificate.

GitHub Actions runs the full regeneration workflow on every push and pull request. Its
successful log is the clean Linux environment record for the repository commit.

## Layout

- `artifacts/`: final binary certificates and rule book.
- `src/`: C verifiers/generators and Python reference/verifier code.
- `rules/`: human-readable Black rule book used by the strategy certificate.
- `paper/`: LaTeX source for the accompanying paper.
- `scripts/`: reproducibility entry point.

## Related implementation

The playable game UI and TypeScript rules engine live in the companion repository
[three-piece-siege-game](https://github.com/KuangjiuWang/three-piece-siege-game).

## License

Code in `src/` and `scripts/` is released under the MIT License. The certificate files,
rule book, and paper sources are released under CC BY 4.0; see `LICENSE-ARTIFACTS.md`.
