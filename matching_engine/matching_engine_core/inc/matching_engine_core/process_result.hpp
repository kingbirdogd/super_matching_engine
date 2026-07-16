#pragma once

#include <matching_engine_core/output_message.hpp>

#include <string>
#include <vector>

namespace matching_engine {

struct ProcessResult {
    std::vector<OutputMessage> outputs;
    std::vector<std::string> errors;
};

}  // namespace matching_engine
