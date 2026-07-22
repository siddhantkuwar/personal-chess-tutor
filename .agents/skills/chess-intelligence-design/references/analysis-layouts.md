# Analysis Layout References

## A: Split inspector

```text
Sidebar | Eval + Board | Move list
                         Explanation
```

## B: Tabbed inspector

```text
Sidebar | Eval + Board | [Review] [Moves] [Engine]
```

Keep current move/verdict visible above tabs.

## C: Beginner stack

```text
Sidebar | Eval + Board | Verdict
                         Explanation
                         Move list
```

## Invariants

- Board gets the largest stable area.
- Eval bar visually belongs to board.
- Current move appears on board and list.
- Opening/accuracy belong in the game header.
- Variation changes context without replacing the screen.
- Engine data is secondary by default.
