#include <matching_engine_core/add_order_request.hpp>
#include <matching_engine_core/cancel_order_request.hpp>
#include <matching_engine_core/line_matching_engine.hpp>
#include <matching_engine_core/process_result.hpp>
#include <matching_engine_core/side.hpp>

#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

namespace matching_engine {

std::string_view LineMatchingEngine::trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

LineMatchingEngine::CsvFields LineMatchingEngine::split_csv(std::string_view line) {
    CsvFields fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t comma = line.find(',', start);
        if (comma == std::string_view::npos) {
            fields.push_back(trim(line.substr(start)));
            break;
        }
        fields.push_back(trim(line.substr(start, comma - start)));
        start = comma + 1;
    }
    return fields;
}

bool LineMatchingEngine::parse_u64(std::string_view text, std::uint64_t& value) {
    text = trim(text);
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    std::uint64_t parsed = 0;
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc() || ptr != end) {
        return false;
    }
    value = parsed;
    return true;
}

bool LineMatchingEngine::parse_price(std::string_view text, long double& value) {
    text = trim(text);
    if (text.empty()) {
        return false;
    }

    std::string owned{text};
    char* end = nullptr;
    const long double parsed = std::strtold(owned.c_str(), &end);
    if (end == owned.c_str() || *end != '\0' || !std::isfinite(parsed)) {
        return false;
    }

    value = parsed;
    return true;
}

ProcessResult LineMatchingEngine::process_line(std::string_view raw_line) {
    ProcessResult result;

    const std::size_t comment_pos = raw_line.find("//");
    const std::string_view no_comment =
        (comment_pos == std::string_view::npos) ? raw_line : raw_line.substr(0, comment_pos);
    const std::string_view line = trim(no_comment);

    if (line.empty()) {
        return result;
    }

    const auto fields = split_csv(line);
    if (fields.empty() || fields[0].empty()) {
        result.errors.emplace_back("Unknown message type: " + std::string(line));
        return result;
    }

    std::uint64_t msg_type = 0;
    if (!parse_u64(fields[0], msg_type)) {
        result.errors.emplace_back("Unknown message type: " + std::string(fields[0]));
        return result;
    }

    if (msg_type == 0U) {
        if (fields.size() != 5U) {
            result.errors.emplace_back("AddOrderRequest expects 5 fields");
            return result;
        }

        AddOrderRequest add{};
        if (!parse_u64(fields[1], add.order_id) || add.order_id == 0U) {
            result.errors.emplace_back("Invalid AddOrderRequest orderid");
            return result;
        }

        std::uint64_t side_raw = 0;
        if (!parse_u64(fields[2], side_raw) || (side_raw != 0U && side_raw != 1U)) {
            result.errors.emplace_back("Invalid AddOrderRequest side");
            return result;
        }
        add.side = (side_raw == 0U) ? Side::Buy : Side::Sell;

        if (!parse_u64(fields[3], add.quantity) || add.quantity == 0U) {
            result.errors.emplace_back("Invalid AddOrderRequest quantity");
            return result;
        }

        if (!parse_price(fields[4], add.price) || add.price <= 0.0L) {
            result.errors.emplace_back("Invalid AddOrderRequest price");
            return result;
        }

        return engine_.handle_add_request(add);
    }

    if (msg_type == 1U) {
        if (fields.size() != 2U) {
            result.errors.emplace_back("CancelOrderRequest expects 2 fields");
            return result;
        }

        CancelOrderRequest cancel{};
        if (!parse_u64(fields[1], cancel.order_id) || cancel.order_id == 0U) {
            result.errors.emplace_back("Invalid CancelOrderRequest orderid");
            return result;
        }

        return engine_.handle_cancel_request(cancel);
    }

    result.errors.emplace_back("Unknown message type: " + std::string(fields[0]));
    return result;
}

void LineMatchingEngine::reset() {
    engine_.reset();
}

}  // namespace matching_engine
