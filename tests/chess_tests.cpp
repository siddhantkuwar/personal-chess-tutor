#include "test.hpp"

#include "pct/chess/board.hpp"
#include "pct/chess/san.hpp"

using namespace pct::chess;

TEST_CASE("initial FEN round trip") {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    CHECK_EQ(Board::from_fen(fen).to_fen(), fen);
}

TEST_CASE("FEN rejects malformed boards") {
    CHECK_THROWS(Board::from_fen("8/8/8/8/8/8/8/8 w - - 0 1"));
    CHECK_THROWS(Board::from_fen("9/8/8/8/8/8/8/K6k w - - 0 1"));
}

TEST_CASE("initial position perft") {
    Board board = Board::initial();
    CHECK_EQ(board.perft(1), 20ULL);
    CHECK_EQ(board.perft(2), 400ULL);
    CHECK_EQ(board.perft(3), 8902ULL);
    CHECK_EQ(board.perft(4), 197281ULL);
}

TEST_CASE("kiwipete perft covers castling") {
    Board board =
        Board::from_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    CHECK_EQ(board.perft(1), 48ULL);
    CHECK_EQ(board.perft(2), 2039ULL);
    CHECK_EQ(board.perft(3), 97862ULL);
}

TEST_CASE("make and unmake restore all state") {
    Board board = Board::initial();
    const Board original = board;
    for (const Move& move : board.legal_moves()) {
        const Undo undo = board.make_move(move);
        board.unmake_move(move, undo);
        CHECK(board == original);
    }
}

TEST_CASE("en passant removes and restores the captured pawn") {
    Board board = Board::from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2");
    const Board original = board;
    const auto move = board.find_legal_move(parse_square("e5"), parse_square("d6"));
    CHECK(move.has_value());
    CHECK(move->has(EnPassant));
    const Undo undo = board.make_move(*move);
    CHECK(board.at(parse_square("d5")).empty());
    board.unmake_move(*move, undo);
    CHECK(board == original);
}

TEST_CASE("promotion creates selected piece") {
    Board board = Board::from_fen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    const auto move =
        board.find_legal_move(parse_square("a7"), parse_square("a8"), PieceType::Knight);
    CHECK(move.has_value());
    board.make_move(*move);
    CHECK(board.at(parse_square("a8")) == (Piece{Color::White, PieceType::Knight}));
}

TEST_CASE("SAN round trips a tactical opening") {
    Board board = Board::initial();
    for (const std::string san : {"e4", "e5", "Nf3", "Nc6", "Bb5", "a6", "Bxc6", "dxc6", "O-O"}) {
        const Move move = parse_san(board, san);
        CHECK_EQ(to_san(board, move), san);
        board.make_move(move);
    }
}

TEST_CASE("hash is stable after unmake") {
    Board board = Board::initial();
    const auto initial_hash = board.hash();
    const Move move = parse_san(board, "e4");
    const Undo undo = board.make_move(move);
    CHECK(board.hash() != initial_hash);
    board.unmake_move(move, undo);
    CHECK_EQ(board.hash(), initial_hash);
}

TEST_CASE("incremental Zobrist hash matches full recomputation") {
    Board board = Board::initial();
    CHECK(board.hash_is_consistent());
    for (int ply = 0; ply < 32; ++ply) {
        const auto moves = board.legal_moves();
        if (moves.empty())
            break;
        static_cast<void>(board.make_move(moves[static_cast<std::size_t>(ply) % moves.size()]));
        CHECK(board.hash_is_consistent());
    }
}

TEST_CASE("repetition history detects threefold positions") {
    Board board = Board::initial();
    for (int cycle = 0; cycle < 2; ++cycle) {
        for (const std::string san : {"Nf3", "Nf6", "Ng1", "Ng8"}) {
            board.make_move(parse_san(board, san));
        }
    }
    CHECK_EQ(board.repetition_count(), 3ULL);
    CHECK(board.is_threefold_repetition());
}

TEST_CASE("multi-ply make and unmake is lossless") {
    Board board = Board::initial();
    const Board initial = board;
    std::vector<std::pair<Move, Undo>> history;
    for (int ply = 0; ply < 24; ++ply) {
        const auto moves = board.legal_moves();
        CHECK(!moves.empty());
        const Move move =
            moves[static_cast<std::size_t>((ply * 17) % static_cast<int>(moves.size()))];
        history.emplace_back(move, board.make_move(move));
        CHECK_EQ(Board::from_fen(board.to_fen()).to_fen(), board.to_fen());
    }
    while (!history.empty()) {
        board.unmake_move(history.back().first, history.back().second);
        history.pop_back();
    }
    CHECK(board == initial);
}
