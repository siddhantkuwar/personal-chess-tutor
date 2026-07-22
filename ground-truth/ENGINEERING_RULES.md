# Engineering Rules

## Repository first

Inspect the real repo before choosing build commands, package manager, state library, board library, API transport, tests, formatting, C++ standard, or packaging.

## Refactoring

- Incremental, runnable stages
- Separate mechanical moves from behavior changes
- Avoid changing both sides of an API at once without end-to-end tests
- Preserve behavior unless intentionally superseded
- Remove dead paths only after proving they are dead
- Avoid parallel old/new systems without a removal plan

## C++

RAII, explicit ownership, cooperative cancellation, checked external parsing, structured errors, versioned persisted analysis, no casual global mutable state, sanitizers, measure before optimization.

## React

Render domain state; do not recreate algorithms. Use typed contracts, centralized tokens, reusable primitives, accessible semantics, and a clear separation between runtime state and local UI state.

## APIs

Explicit status enums, IDs in progress events, structured error codes, versioned breaking changes, no parsing human-readable logs in React.

## Performance

Move navigation and board input stay immediate. Analysis never blocks UI. Progress updates avoid render storms. Variation analysis is lower priority than explicit review.

## Testing

Run existing tests before/after. Add contract, state-transition, variation, theme, keyboard, critical-flow, and error tests.

## Visual verification

Run the real app with Playwright or equivalent, both themes and supported widths. A passing TypeScript build proves only that the compiler has no aesthetic jurisdiction.

## Security/privacy

Treat remote content as untrusted, avoid shell construction from input, validate URLs/usernames, bind local servers safely, and upload no games or telemetry by default.
