#include <matching_engine_core/matching_engine.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <list>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace matching_engine {

namespace {

using Price = long double;
using OrderId = std::uint64_t;
using Quantity = std::uint64_t;
using Level = std::list<OrderId>;
using BidBook = std::map<Price, Level, std::greater<Price>>;
using AskBook = std::map<Price, Level, std::less<Price>>;

struct ParsedAdd {
    OrderId order_id{0};
    Side side{Side::Buy};
    Quantity quantity{0};
    Price price{0.0L};
};

struct EngineOrder {
    OrderId id{0};
    Side side{Side::Buy};
    Quantity remaining{0};
    Price price{0.0L};
    Level::iterator level_it{};
};

std::string format_price_value(long double value) {
    std::ostringstream out;
    out << std::setprecision(18) << std::fixed << static_cast<double>(value);
    std::string text = out.str();
    while (text.size() > 1U && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

}  // namespace

class BookState final {
public:
    std::unordered_map<OrderId, EngineOrder> orders;
    BidBook bids;
    AskBook asks;
};

static BookState& state() {
    static BookState value;
    return value;
}

std::string OutputMessage::to_csv() const {
    std::ostringstream out;
    switch (type) {
        case OutputType::TradeEvent:
            out << "2," << quantity << ',' << format_price_value(price);
            break;
        case OutputType::OrderFullyFilled:
            out << "3," << order_id;
            break;
        case OutputType::OrderPartiallyFilled:
            out << "4," << order_id << ',' << quantity;
            break;
        default:
            out << "2,0,0";
            break;
    }
    return out.str();
}

std::string_view MatchingEngine::trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

std::vector<std::string_view> MatchingEngine::split_csv(std::string_view line) {
    std::vector<std::string_view> fields;
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

bool MatchingEngine::parse_u64(std::string_view text, std::uint64_t& value) {
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

bool MatchingEngine::parse_price(std::string_view text, long double& value) {
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

ProcessResult MatchingEngine::process_line(std::string_view raw_line) {
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
        return handle_add_request(fields);
    }
    if (msg_type == 1U) {
        return handle_cancel_request(fields);
    }

    result.errors.emplace_back("Unknown message type: " + std::string(fields[0]));
    return result;
}

ProcessResult MatchingEngine::handle_add_request(const std::vector<std::string_view>& fields) {
    ProcessResult result;
    ParsedAdd add{};
    if (fields.size() != 5U) {
        result.errors.emplace_back("AddOrderRequest expects 5 fields");
        return result;
    }

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

    BookState& book = state();
    if (book.orders.contains(add.order_id)) {
        result.errors.emplace_back("Duplicate orderid: " + std::to_string(add.order_id));
        return result;
    }

    EngineOrder incoming{};
    incoming.id = add.order_id;
    incoming.side = add.side;
    incoming.remaining = add.quantity;
    incoming.price = add.price;

    auto emit_trade = [&](Quantity traded_quantity, Price trade_price) {
        OutputMessage trade{};
        trade.type = OutputType::TradeEvent;
        trade.quantity = traded_quantity;
        trade.price = trade_price;
        result.outputs.push_back(trade);

        OutputMessage aggressive{};
        aggressive.order_id = incoming.id;
        if (incoming.remaining == 0U) {
            aggressive.type = OutputType::OrderFullyFilled;
        } else {
            aggressive.type = OutputType::OrderPartiallyFilled;
            aggressive.quantity = incoming.remaining;
        }
        result.outputs.push_back(aggressive);
    };

    if (incoming.side == Side::Buy) {
        for (auto level_it = book.asks.begin();
             level_it != book.asks.end() && incoming.remaining > 0U;) {
            if (level_it->first > incoming.price) {
                break;
            }

            auto& queue = level_it->second;
            while (!queue.empty() && incoming.remaining > 0U) {
                const OrderId resting_id = queue.front();
                auto resting_it = book.orders.find(resting_id);
                if (resting_it == book.orders.end()) {
                    queue.pop_front();
                    continue;
                }

                EngineOrder& resting = resting_it->second;
                const Quantity traded = std::min(incoming.remaining, resting.remaining);
                incoming.remaining -= traded;
                resting.remaining -= traded;

                emit_trade(traded, resting.price);

                OutputMessage resting_msg{};
                resting_msg.order_id = resting.id;
                if (resting.remaining == 0U) {
                    resting_msg.type = OutputType::OrderFullyFilled;
                    queue.pop_front();
                    book.orders.erase(resting_it);
                } else {
                    resting_msg.type = OutputType::OrderPartiallyFilled;
                    resting_msg.quantity = resting.remaining;
                }
                result.outputs.push_back(resting_msg);
            }

            if (queue.empty()) {
                level_it = book.asks.erase(level_it);
            } else {
                ++level_it;
            }
        }
    } else {
        for (auto level_it = book.bids.begin();
             level_it != book.bids.end() && incoming.remaining > 0U;) {
            if (level_it->first < incoming.price) {
                break;
            }

            auto& queue = level_it->second;
            while (!queue.empty() && incoming.remaining > 0U) {
                const OrderId resting_id = queue.front();
                auto resting_it = book.orders.find(resting_id);
                if (resting_it == book.orders.end()) {
                    queue.pop_front();
                    continue;
                }

                EngineOrder& resting = resting_it->second;
                const Quantity traded = std::min(incoming.remaining, resting.remaining);
                incoming.remaining -= traded;
                resting.remaining -= traded;

                emit_trade(traded, resting.price);

                OutputMessage resting_msg{};
                resting_msg.order_id = resting.id;
                if (resting.remaining == 0U) {
                    resting_msg.type = OutputType::OrderFullyFilled;
                    queue.pop_front();
                    book.orders.erase(resting_it);
                } else {
                    resting_msg.type = OutputType::OrderPartiallyFilled;
                    resting_msg.quantity = resting.remaining;
                }
                result.outputs.push_back(resting_msg);
            }

            if (queue.empty()) {
                level_it = book.bids.erase(level_it);
            } else {
                ++level_it;
            }
        }
    }

    if (incoming.remaining > 0U) {
        auto [it, inserted] = book.orders.emplace(incoming.id, incoming);
        if (!inserted) {
            result.errors.emplace_back("Failed to insert orderid: " + std::to_string(incoming.id));
            return result;
        }

        EngineOrder& stored = it->second;
        if (stored.side == Side::Buy) {
            auto [price_it, _] = book.bids.try_emplace(stored.price);
            price_it->second.push_back(stored.id);
            stored.level_it = std::prev(price_it->second.end());
        } else {
            auto [price_it, _] = book.asks.try_emplace(stored.price);
            price_it->second.push_back(stored.id);
            stored.level_it = std::prev(price_it->second.end());
        }
    }

    return result;
}

ProcessResult MatchingEngine::handle_cancel_request(const std::vector<std::string_view>& fields) {
    ProcessResult result;

    if (fields.size() != 2U) {
        result.errors.emplace_back("CancelOrderRequest expects 2 fields");
        return result;
    }

    std::uint64_t order_id = 0;
    if (!parse_u64(fields[1], order_id) || order_id == 0U) {
        result.errors.emplace_back("Invalid CancelOrderRequest orderid");
        return result;
    }

    BookState& book = state();
    auto order_it = book.orders.find(order_id);
    if (order_it == book.orders.end()) {
        result.errors.emplace_back("Unknown orderid for cancel: " + std::to_string(order_id));
        return result;
    }

    EngineOrder& order = order_it->second;

    if (order.side == Side::Buy) {
        auto price_it = book.bids.find(order.price);
        if (price_it != book.bids.end()) {
            price_it->second.erase(order.level_it);
            if (price_it->second.empty()) {
                book.bids.erase(price_it);
            }
        }
    } else {
        auto price_it = book.asks.find(order.price);
        if (price_it != book.asks.end()) {
            price_it->second.erase(order.level_it);
            if (price_it->second.empty()) {
                book.asks.erase(price_it);
            }
        }
    }

    book.orders.erase(order_it);
    return result;
}

}  // namespace matching_engine
