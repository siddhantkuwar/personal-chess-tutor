#include <iostream>
#include <string>

int main() {
    for (std::string command; std::getline(std::cin, command);) {
        if (command == "uci") {
            std::cout << "id name PCT Slow Engine\nuciok\n" << std::flush;
        } else if (command == "isready") {
            std::cout << "readyok\n" << std::flush;
        } else if (command == "stop") {
            std::cout << "bestmove e2e4\n" << std::flush;
        } else if (command == "quit") {
            return 0;
        }
    }
}
