# Component Specification

## Shell

`AppShell`: sidebar, contextual header, route content, background analysis status, command palette, notification region.

`Sidebar`: Recent Games, Analysis, Explore, Progress, Settings. Support a compact mode and visible active state.

## Recent Games

`RecentGameRow`: players, user color, result, ratings, time control, date, opening, and analysis status.

`GameSelectionToolbar`: analyze selected, cancel queued, clear selection.

## Analysis

`BoardFrame`: canonical/variation position, highlights, arrows, last move, best move, pattern overlays, retry state, orientation.

`EvaluationBar`: stable scale, mate state, accessible text, optional number, restrained transitions.

`MoveList`: compact scan, move grouping, classification, current move, keyboard navigation, canonical/variation distinction, controlled auto-scroll.

`ReviewInspector`: main explanation, best move, opening, pattern, engine line, technical detail. One section dominates at a time.

`AnalysisProgress`: actual stages, counts, batch progress, cancel, retry, failures.

`AccuracySummary`, `OpeningIdentity`, `PatternDetectedButton`, `VariationBreadcrumb`, and optional `VariationTree`.

## Explore

`LearningRecommendation` must state why it is recommended: recent game, repeated weakness, repertoire relevance, or due review.

## Progress

`PersonalPatternRow`, `SkillProfileSection`, and `InsightSentence`. Every insight links to supporting positions or metrics.

## Component states

Default, hover, focus-visible, active, disabled, loading, error, empty, selected. Use explicit states rather than a spinner floating in the void.
