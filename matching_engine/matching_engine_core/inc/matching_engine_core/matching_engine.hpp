#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace matching_engine {

enum class Side : std::uint8_t {
    Buy = 0,
    Sell = 1,
};

enum class OutputType : std::uint8_t {
    TradeEvent = 2,
    OrderFullyFilled = 3,
    OrderPartiallyFilled = 4,
};

struct OutputMessage {
    OutputType type{OutputType::TradeEvent};
    std::uint64_t order_id{0};
    std::uint64_t quantity{0};
    long double price{0.0L};

    [[nodiscard]] std::string to_csv() const;
};

struct ProcessResult {
    std::vector<OutputMessage> outputs;
    std::vector<std::string> errors;
};

class MatchingEngine {
public:
    MatchingEngine() = default;
    ~MatchingEngine() = default;

    [[nodiscard]] ProcessResult process_line(std::string_view line);

private:
    ProcessResult handle_add_request(const std::vector<std::string_view>& fields);
    ProcessResult handle_cancel_request(const std::vector<std::string_view>& fields);

    static std::string_view trim(std::string_view value);
    static std::vector<std::string_view> split_csv(std::string_view line);
    static bool parse_u64(std::string_view text, std::uint64_t& value);
    static bool parse_price(std::string_view text, long double& value);
};

}  // namespace matching_engine
