#include <iostream>
#include <string>

int main() {
    for (std::string command; std::getline(std::cin, command);) {
        if (command == "uci") {
            std::cout << "id name PCT Crash Engine\nuciok\n" << std::flush;
        } else if (command == "isready") {
            std::cout << "readyok\n" << std::flush;
        } else if (command.starts_with("go ")) {
            return 9;
        } else if (command == "quit") {
            return 0;
        }
    }
}
