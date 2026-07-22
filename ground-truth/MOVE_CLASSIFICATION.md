# Move Classification

## Objective

Use familiar chess vocabulary with an independent, transparent, versioned C++ implementation and original visual identity.

Possible labels:

- Brilliant
- Great
- Best
- Excellent
- Good
- Book
- Inaccuracy
- Mistake
- Miss
- Blunder

Do not copy Chess.com's glyphs, exact palette, copy, UI, or algorithm.

## Inputs

- Engine rank of played move
- Evaluation or mate-score change
- Change in expected result/winning chances
- Material and tactical consequences
- Forced-move status
- Book membership
- Position phase
- Player rating when available

## Ordinary labels

`Book`: selected opening source and acceptable engine tolerance.

`Best`: engine top choice with deterministic tie handling.

`Excellent`: near-best and preserves the practical position.

`Good`: sound but modestly inferior.

`Inaccuracy`: meaningful, usually recoverable damage.

`Mistake`: significant positional/material loss or likely-result change.

`Blunder`: severe loss, allowed mate, missed forced defense, decisive material loss, or winning/drawn to losing transition.

## Special labels

`Great`: unusually important, such as the only move preserving the result or a difficult defensive resource.

`Brilliant`: best or near-best, sound sacrifice/counterintuitive resource, non-forced, not a routine recapture, and materially stronger than natural alternatives. Verify deeply enough and avoid handing out genius stickers for forced queen recaptures.

`Miss`: a concrete winning/equalizing opportunity created by the opponent was not exploited.

## Expected-result model

Centipawn loss behaves poorly near mate and in already won positions. Normalize to expected result or winning chance where practical. Document the formula and classifier version.

## Accuracy

Define independently:

- 0–100 display
- Mate handling
- Book treatment
- Sample size
- Scoring version
- No claim of equivalence to Chess.com's Accuracy

## Evidence

Return explanation evidence, not only a label:

```text
Blunder
Evaluation changed from +1.3 to -2.4.
The move left the bishop undefended and Black can win it immediately.
```

## Testing

Fixtures for top move, equal alternatives, forced recapture, sound/unsound sacrifices, missed mate, only defense, book transposition, already won positions, and mate transitions.
