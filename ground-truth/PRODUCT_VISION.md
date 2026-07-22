# Product Vision

## Definition

This is a **local chess intelligence platform**, not merely an analysis board and not a chatbot with a knight icon.

It gives users private, unlimited analysis of Chess.com games. Over time, it turns reviews into a model of the player's habits, strengths, weaknesses, openings, and pattern knowledge.

## Users

- Beginners who need plain explanations and protection from engine overload
- Improving players who want unlimited analysis without a subscription
- Streamers and club players who want a polished workstation and advanced controls
- Systems recruiters evaluating the C++ runtime, concurrency, storage, testing, and UI boundary

## Promise

The user can:

- Import a Chess.com game by URL
- Refresh recent games in a future phase
- Analyze one game or a batch locally
- Review every move with an independently defined classification
- See opening, accuracy, evaluation, and best continuation
- Hide or reveal technical engine detail
- Retry a mistake before revealing the answer
- Branch from any position and continue legal moves indefinitely
- Learn detected tactical, positional, opening, and endgame patterns
- See which patterns and failures are personal and recurring
- Track rating and skill development

## Pillars

1. **Unlimited local analysis**: limits come from local compute and chosen settings, not daily quotas.
2. **Board-centered intelligence**: explanations appear at the exact move and position where they matter.
3. **Progressive disclosure**: beginners see conclusions; advanced users reveal engine depth and lines.
4. **Personal memory**: every analyzed game can improve future review and training.
5. **Systems credibility**: the project demonstrates C++ process management, scheduling, caching, persistence, determinism, profiling, and robust APIs.

## Non-goals for this redesign

- Online chess play
- Social features
- A cloud LLM
- Replacing Stockfish
- Copying Chess.com or ChatGPT
- Mobile-first design
- Finishing 190+ pattern detectors before the core review loop works

## Success

- A new user can import and analyze without instructions.
- A beginner understands the main mistake without reading engine notation.
- An advanced user can reveal technical detail without leaving the screen.
- Variations never corrupt the imported game.
- Existing analysis behavior survives the redesign.
- React never becomes a second chess engine.
