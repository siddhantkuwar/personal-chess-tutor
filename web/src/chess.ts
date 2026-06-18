const pieces: Record<string, string> = {
  K: "♔",
  Q: "♕",
  R: "♖",
  B: "♗",
  N: "♘",
  P: "♙",
  k: "♚",
  q: "♛",
  r: "♜",
  b: "♝",
  n: "♞",
  p: "♟",
};

export interface BoardSquare {
  name: string;
  piece: string;
  file: string;
  rank: string;
}

export function squaresFromFen(fen: string): BoardSquare[] {
  const placement = fen.split(" ")[0] ?? "8/8/8/8/8/8/8/8";
  const ranks = placement.split("/");
  const squares: BoardSquare[] = [];
  ranks.forEach((rankData, rankIndex) => {
    let file = 0;
    for (const value of rankData) {
      if (/\d/.test(value)) {
        const count = Number(value);
        for (let offset = 0; offset < count; offset += 1) {
          squares.push(square(file + offset, 7 - rankIndex, ""));
        }
        file += count;
      } else {
        squares.push(square(file, 7 - rankIndex, pieces[value] ?? ""));
        file += 1;
      }
    }
  });
  return squares;
}

function square(file: number, rank: number, piece: string): BoardSquare {
  const fileName = String.fromCharCode(97 + file);
  return { name: `${fileName}${rank + 1}`, piece, file: fileName, rank: String(rank + 1) };
}

export function uciSquares(uci: string): [string, string] | null {
  return /^[a-h][1-8][a-h][1-8]/.test(uci) ? [uci.slice(0, 2), uci.slice(2, 4)] : null;
}
