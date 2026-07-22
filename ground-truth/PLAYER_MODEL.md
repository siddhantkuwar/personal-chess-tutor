# Player Model and Progress

## Purpose

Progress is a profile of the user's chess, not a pile of decorative charts.

## Sources

Chess.com public profile/rating data, imported PGNs, local reviews, classifications, openings, patterns, retry/training outcomes, and review behavior.

## Profile

### Rating

Current and historical rating by time control, peak, recent change, game count, and freshness. Never fabricate unavailable ratings.

### Move quality

Accuracy, errors per game, average evaluation loss, first serious error, and performance when winning/equal/losing.

### Game phase

Opening, middlegame, endgame, with sample size and trend.

### Openings

Structure, color, results, evaluation after key ranges, common personal deviation, repeated errors, and relevant lessons.

### Patterns

Seen, missed, successfully used, impact, trend, practice history, and mastery.

## Weakness taxonomy

Examples: hanging pieces, ignored threats, unsafe king, premature pawn pushes, bad trades while behind, missed tactics, passed-pawn handling, back-rank safety, delayed development, time pressure.

A weakness aggregates repeated evidence and links to positions.

## Deterministic insights

Examples:

> In your last 20 analyzed rapid games, 11 of 17 major errors occurred before move 12.

> Italian Game results improved, but delaying castling still causes repeated evaluation loss.

Every sentence must be traceable to structured data.

## Confidence

Account for sample size, recency, time control, detector confidence, and version compatibility. Do not declare endgame mastery after two endings because the graph looked lonely.

## Privacy

Local by default, no telemetry upload, explicit export/deletion, and clear disclosure of public Chess.com data retrieval.
