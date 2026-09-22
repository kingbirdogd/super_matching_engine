#pragma once

#include <matching_engine_core/output_message.hpp>
#include <matching_engine_core/fixed_pool_allocator.hpp>

#include <string>
#include <vector>

namespace matching_engine {

inline constexpr std::size_t kMaxProcessOutputs = 4096U;
inline constexpr std::size_t kMaxProcessErrors = 64U;

struct ProcessResult {
    std::vector<OutputMessage, FixedPoolAllocator<OutputMessage, kMaxProcessOutputs>> outputs;
    std::vector<std::string, FixedPoolAllocator<std::string, kMaxProcessErrors>> errors;
};

}  // namespace matching_engine
