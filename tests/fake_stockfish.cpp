#include <iostream>
#include <string>

int main() {
    for (std::string command; std::getline(std::cin, command);) {
        if (command == "uci") {
            std::cout << "id name PCT Fake Stockfish\nuciok\n" << std::flush;
        } else if (command == "isready") {
            std::cout << "readyok\n" << std::flush;
        } else if (command.starts_with("go ")) {
            std::cout << "info depth 8 multipv 1 score cp 160 nodes 1200 time 5 pv e2e4 e7e5\n";
            std::cout << "info depth 8 multipv 2 score cp 145 nodes 1100 time 5 pv d2d4 d7d5\n";
            std::cout << "bestmove e2e4 ponder e7e5\n" << std::flush;
        } else if (command == "stop") {
            std::cout << "bestmove e2e4\n" << std::flush;
        } else if (command == "quit") {
            return 0;
        }
    }
}
