import { moveOverlayGeometry, parseUciMove, squareGeometry } from "../src/chess";

function equal(actual: unknown, expected: unknown, label: string): void {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(`${label}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
  }
}

equal(parseUciMove("e7e8q"), { from: "e7", to: "e8", promotion: "q" }, "promotion parsing");
equal(parseUciMove("e4d5"), { from: "e4", to: "d5", promotion: null }, "capture-shaped move parsing");
equal(squareGeometry("a8", "white"), { column: 0, row: 0, size: 12.5, x: 6.25, y: 6.25 }, "white orientation a8");
equal(squareGeometry("a8", "black"), { column: 7, row: 7, size: 12.5, x: 93.75, y: 93.75 }, "black orientation a8");
equal(moveOverlayGeometry("c3d4", "white"), {
  from: "c3",
  to: "d4",
  promotion: null,
  source: { column: 2, row: 5, size: 12.5, x: 31.25, y: 68.75 },
  destination: { column: 3, row: 4, size: 12.5, x: 43.75, y: 56.25 },
}, "white overlay");
equal(moveOverlayGeometry("c3d4", "black"), {
  from: "c3",
  to: "d4",
  promotion: null,
  source: { column: 5, row: 2, size: 12.5, x: 68.75, y: 31.25 },
  destination: { column: 4, row: 3, size: 12.5, x: 56.25, y: 43.75 },
}, "flipped overlay");

console.log("chess geometry tests passed");
