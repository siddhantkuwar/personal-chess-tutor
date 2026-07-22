# UX Architecture

## Navigation

```text
Recent Games
Analysis
Explore
Progress
Settings
```

Use a restrained desktop sidebar. A command palette may accelerate import, refresh, analyze, resume review, search, and settings, but must not be the only route.

## Recent Games

Default launch screen. Use a refined list or compact table, not giant cards.

Show opponent, color, result, ratings, time control, date, rating change when available, analysis status, and opening when known.

Primary action: Analyze. Support selection and batch analysis.

## Analysis

The flagship screen, defined in `ANALYSIS_WORKSTATION.md`.

## Explore

Organize around personal relevance:

```text
For You
Openings
Middlegames
Endgames
Patterns
Library
```

Only show lessons backed by real data. Empty states explain how analysis populates the section.

## Progress

A player profile, not a casino scoreboard:

- Rating history and recent form
- Accuracy and move-quality trends
- Phase performance
- Opening profile
- Personal patterns and repeated weaknesses
- Future practice history

## Settings

### Analysis

Search limit, MultiPV, CPU allocation, hash memory, and batch concurrency.

### Review

Engine lines, automatic best-move reveal, retry, variation assistance, technical detail, automatic patterns, explanation depth.

### Coach

Beginner and friendly; Cynical and hard; Devil's advocate.

### Appearance

System/light/dark, board style, pieces, move-class intensity, reduced motion.

### Data

Chess.com username, refresh, export, storage, cache, and deletion.

## Required screen states

Initial loading, empty, partial, success, recoverable error, fatal error, offline, stale synchronized data, background analysis, and cancellation.

## Desktop scope

Primary target 1440×900 or larger; support 1280×800 and a documented minimum near 1180×720. Do not spend the redesign compressing this workstation into a phone.
