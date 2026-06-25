import fs from "node:fs";
import readline from "node:readline";

const tokens = process.argv.slice(2);
const args = {};
for (let index = 0; index < tokens.length; ++index)
  if (tokens[index].startsWith("--")) args[tokens[index].slice(2)] = tokens[index + 1];
const input = args.input;
const output = args.output;
const version = args.version;
const limit = Math.min(50000, Math.max(1, Number(args.limit ?? 5000)));
const minimum = Math.max(0, Number(args["min-rating"] ?? 600));
const maximum = Math.min(4000, Number(args["max-rating"] ?? 2400));
if (!input || !output || !version) {
  console.error("usage: node scripts/import-lichess-puzzles.js --input puzzles.csv --output corpus.json --version YYYY-MM-DD --limit 5000 [--min-rating 600 --max-rating 2400]");
  process.exit(2);
}

const motifNames = new Map([
  ["fork", "Fork"], ["pin", "Pin"], ["skewer", "Skewer"],
  ["discoveredAttack", "Discovered attack"], ["backRankMate", "Back-rank weakness"],
  ["hangingPiece", "Hanging piece"], ["capturingDefender", "Removal of defender"],
  ["deflection", "Removal of defender"], ["overloading", "Overloaded defender"],
  ["mate", "Failed response to mate threat"], ["advancedPawn", "Ignored passed pawn"],
]);
const puzzles = [];
const lines = readline.createInterface({ input: fs.createReadStream(input), crlfDelay: Infinity });
let header = true;
for await (const line of lines) {
  if (header) { header = false; continue; }
  if (puzzles.length >= limit) break;
  const columns = line.split(",");
  if (columns.length < 8) continue;
  const [id, fen, movesText, ratingText, , popularityText, , themesText] = columns;
  const rating = Number(ratingText);
  const popularity = Number(popularityText);
  const motifs = themesText.split(" ").map((theme) => motifNames.get(theme)).filter(Boolean);
  const moves = movesText.split(" ").filter(Boolean);
  if (rating < minimum || rating > maximum || popularity < 75 || motifs.length === 0 || moves.length < 2) continue;
  puzzles.push({ id, fen, moves, rating, motifs: [...new Set(motifs)] });
}
const document = {
  manifest: {
    id: "lichess-puzzles-local-selection", version,
    source_url: "https://database.lichess.org/#puzzles",
    download_url: "https://database.lichess.org/lichess_db_puzzle.csv.zst",
    license: "Creative Commons CC0",
  },
  puzzles,
};
fs.writeFileSync(output, `${JSON.stringify(document, null, 2)}\n`, { flag: "wx" });
console.log(`wrote ${puzzles.length} puzzles to ${output}`);
