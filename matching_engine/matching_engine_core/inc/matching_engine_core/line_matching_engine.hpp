#pragma once

#include <matching_engine_core/matching_engine.hpp>
#include <matching_engine_core/process_result.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace matching_engine {

class LineMatchingEngine {
public:
    LineMatchingEngine() = default;
    ~LineMatchingEngine() = default;

    void reset();
    [[nodiscard]] ProcessResult process_line(std::string_view line);

private:
    static std::string_view trim(std::string_view value);
    static std::vector<std::string_view> split_csv(std::string_view line);
    static bool parse_u64(std::string_view text, std::uint64_t& value);
    static bool parse_price(std::string_view text, long double& value);

    MatchingEngine engine_{};
};

}  // namespace matching_engine
