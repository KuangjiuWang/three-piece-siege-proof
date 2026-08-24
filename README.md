# Three-Piece Siege: Proof Artifact

This repository accompanies *Three-Piece Siege on the 4 × 4 Board is a Draw: A Certificate-Based Proof via Safety Games and a Colour-Reversible Rule Book*.

The main result is that, under the stated history-dependent threefold-repetition rule, **both players have a positional strategy guaranteeing at least a draw**. The proof is certificate-based: it does not rely on a full game-value table.

## Main certificate

- `artifacts/safe6.bin` — main safety certificate, containing 91,330 certified positions.
- `src/certify.c` — independent local verifier for legality, safety closure, the initial positions, and the colour-reversed certificate.

The certificate is checked locally over the full 3,723,720-position universe; the published verification takes about 0.12 s in the reference environment.

## Rule book

- `rules/black-rule-book.md` — human-readable 970-rule memoryless strategy.
- `artifacts/book_dual.pkl` — machine-readable version of the rule book.
- `src/python/dualbook.py` — rule-book interpreter.
- `src/python/verify_dual.py` — exhaustive verifier for both colour-conjugate uses of the book.
- `artifacts/sigma.bin` — smaller 30,245-position certificate induced by the rule book.

## Auxiliary files

- `src/python/engine.py` / `test_engine.py` — reference rules engine and tests.
- `src/safety6.c` / `src/python/prune3.py` — programs used to construct and simplify the certificate.
- `src/solve.c` / `artifacts/values.bin` — auxiliary history-free retrograde classification; **not** the main proof.
- `artifacts/allowed2.bin` — auxiliary shape/index data.

## Paper

- `paper/siege4x4.tex` — current LaTeX source.
- `paper/siege4x4.pdf` — compiled paper.

## Reproduce

On Linux with Bash, Python 3, and a C compiler:

```bash
bash scripts/verify.sh
```

This checks the published digests, builds the verifier, checks the certificates, runs the rule-engine tests, and verifies the dual rule book. See `REPRODUCTION.md` for details.

## License

Code is released under the MIT License. The paper, certificates, and rule book are released under CC BY 4.0; see `LICENSE-ARTIFACTS.md`.