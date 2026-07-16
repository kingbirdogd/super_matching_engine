#include <matching_engine_core/line_matching_engine.hpp>

#include <exception>
#include <iostream>
#include <string>

int main() {
    matching_engine::LineMatchingEngine engine;

    std::string line;
    while (std::getline(std::cin, line)) {
        matching_engine::ProcessResult result = engine.process_line(line);

        for (const auto& msg : result.outputs) {
            std::cout << msg.to_csv() << '\n';
        }
        for (const auto& err : result.errors) {
            std::cerr << err << '\n';
        }
    }

    if (!std::cin.eof() && std::cin.fail()) {
        std::cerr << "Failed to read stdin" << '\n';
        return 1;
    }

    return 0;
}
