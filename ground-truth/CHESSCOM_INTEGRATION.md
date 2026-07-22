# Chess.com Integration

## Current

Preserve the working import-by-game-link flow.

## Future refresh

Use Chess.com's public read-only PubAPI rather than scraping:

1. Store username locally.
2. Request archive list.
3. Request relevant recent months.
4. Compare stable game IDs/URLs with local records.
5. Import unseen games.
6. Display them in Recent Games.
7. Analyze only after user action.

## Operational behavior

Official Chess.com documentation says the PubAPI is read-only, public data may be cached for up to roughly 12 hours, serial access is unlimited, parallel requests can receive 429, and a descriptive User-Agent with contact information is recommended.

Therefore:

- Serialize or conservatively bound requests.
- Use ETag/Last-Modified when available.
- Back off on 429 and transient failures.
- Identify the app in User-Agent.
- Show data freshness.
- Do not promise instant synchronization.

## Data

Normalize PGN, game URL, players, ratings, time control, result, end time, variant, and available opening metadata. Validate before analysis.

## Brand/IP boundary

Do not copy Chess.com board palettes, pieces, sounds, move-classification glyphs, logo, UI layout, product copy, or branded assets. Public game data does not grant rights to product design.

## Errors

Unknown username, no public games, stale upstream data, rate limit, offline, malformed response, unsupported variant, and duplicate import.

## Privacy

Username is local; local reviews are not sent to Chess.com; linked-profile data can be removed.
