#include "test.hpp"

#include <iostream>

namespace pct::test {

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

Register::Register(std::string name, std::function<void()> run) {
    registry().push_back(Case{std::move(name), std::move(run)});
}

} // namespace pct::test

int main() {
    std::size_t failures = 0;
    for (const auto& test : pct::test::registry()) {
        try {
            test.run();
            std::cout << "PASS " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
    }
    std::cout << pct::test::registry().size() - failures << '/' << pct::test::registry().size()
              << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
