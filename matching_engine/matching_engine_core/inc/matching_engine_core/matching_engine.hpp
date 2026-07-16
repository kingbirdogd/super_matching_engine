#pragma once

#include <matching_engine_core/add_order_request.hpp>
#include <matching_engine_core/cancel_order_request.hpp>
#include <matching_engine_core/process_result.hpp>

namespace matching_engine {

class MatchingEngine {
public:
    MatchingEngine() = default;
    ~MatchingEngine() = default;

    void reset();
    [[nodiscard]] ProcessResult handle_add_request(const AddOrderRequest& request);
    [[nodiscard]] ProcessResult handle_cancel_request(const CancelOrderRequest& request);

private:
    struct Impl;
    static Impl& state();
};

}  // namespace matching_engine
