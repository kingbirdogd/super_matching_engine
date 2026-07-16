#pragma once

#include <matching_engine_core/output_type.hpp>

#include <cstdint>
#include <string>

namespace matching_engine {

struct OutputMessage {
    OutputType type{OutputType::TradeEvent};
    std::uint64_t order_id{0};
    std::uint64_t quantity{0};
    long double price{0.0L};

    [[nodiscard]] std::string to_csv() const;
};

}  // namespace matching_engine
