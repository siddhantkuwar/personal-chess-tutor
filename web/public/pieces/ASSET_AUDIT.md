# Phase 2.2 piece-set audition

Audited 2026-07-19 for legibility at desktop and 390 px mobile board sizes,
silhouette consistency, visual weight on the charcoal/stone board, and licensing.

| Candidate | Source and license | Result |
| --- | --- | --- |
| Lasker | Eight Squared Software, pinned source in `lasker/ATTRIBUTION.md`, CC BY 4.0 | **Selected.** The open counters, restrained detail, and consistent stroke weight remain readable down to the mobile square size. The full pinned source and license are already vendored. |
| cburnett | Lichess `lila` assets, distributed with the AGPL-3.0 project | Rejected for this phase. It is extremely legible, but the copyleft and asset-level provenance need a separate legal decision before vendoring; the visual improvement over Lasker was not decisive. |
| Wikipedia set from chessboard.js | `oakmac/chessboardjs` default piece theme, MIT project | Rejected for this phase. The raster look is familiar but less crisp on high-density displays, and the upstream image-specific attribution would need to be preserved separately from the MIT application license. |

Decision: keep the licensed Lasker SVGs and improve their board scale, contrast, and
shadow treatment. No user-supplied PNG files are needed. Revisit only if a future
candidate is both visibly superior in the actual product and has unambiguous
asset-level redistribution terms.
