# Pattern Intelligence

## Goal

Recognize tactical, positional, opening, and endgame patterns and explain their meaning in this exact position. The library should support 190+ patterns over time; no universal canonical list needs to be invented for marketing numerology.

## Categories

### Tactical

Fork, pin, skewer, double attack, discovered attack, double check, deflection, decoy, interference, overloaded defender, removal of defender, clearance, zwischenzug, trapped piece, back-rank motif, smothered mate, Greek Gift, mating net.

### Positional

Weak square, outpost, open/semi-open file, isolated/backward/doubled/hanging pawns, bishop pair, bad bishop, space, pawn majority, minority attack, rook on seventh, opposite-side castling, pawn storm.

### Opening and structure

Italian/Giuoco Pianissimo, Carlsbad, IQP, Maroczy Bind, Hedgehog, Stonewall, French chains, Sicilian structures, fianchetto formations. Recognize transposed structures.

### Endgame

Opposition, distant opposition, triangulation, Lucena, Philidor, rook behind passer, outside/protected passer, breakthrough, wrong bishop and rook pawn, promotion race.

## Detection

- Deterministic geometry/features for pins, forks, files, pawn structures, outposts, king safety.
- Engine-assisted validation for sacrifices, mating nets, tactical relevance, and whether a thematic plan is sound now.
- Templates for named structures and endgames.

## Data

Pattern definition:

```text
id, name, category, difficulty, description,
detection_version, required/excluded_features,
visual_annotations, plans, related_patterns,
examples, drill_templates
```

Occurrence:

```text
game_id, ply, fen, pattern_id, confidence,
involved_squares/pieces, engine_evidence,
personal_relevance, detector_version
```

## Pattern Detected panel

1. Name
2. Highlighted pieces/squares
3. What is happening here
4. Why it matters to this player
5. Likely continuations
6. Future plan
7. Personal history
8. Practice action

Prefer: “You missed this overloaded defender in 3 of your last 12 analyzed games.”

Avoid: “Beginners commonly struggle with this.” Generic filler has achieved enough market penetration.

## Selection

Prioritize relevance to the played move, educational value, and confidence. Suppress weak matches; let advanced users inspect all detections.

## Learning lifecycle

Detected → Explained → Practiced → Mastered. Opening a panel does not equal mastery.

## Acceptance

Detectors run in C++; React renders structured evidence; personal history is used when available; visual annotations match evidence; new patterns do not require unrelated UI rewrites.
