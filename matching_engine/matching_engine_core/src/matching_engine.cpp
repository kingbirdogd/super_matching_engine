#include <matching_engine_core/add_order_request.hpp>
#include <matching_engine_core/cancel_order_request.hpp>
#include <matching_engine_core/huge_page.hpp>
#include <matching_engine_core/matching_engine.hpp>
#include <matching_engine_core/output_message.hpp>
#include <matching_engine_core/output_type.hpp>
#include <matching_engine_core/process_result.hpp>
#include <matching_engine_core/side.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <list>
#include <map>
#include <sstream>
#include <string>
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

struct MatchingEngine::Impl final {
    std::unordered_map<OrderId, EngineOrder> orders;
    BidBook bids;
    AskBook asks;
};

MatchingEngine::Impl& MatchingEngine::state() {
    // This reference keeps the startup HugeTLB object linked when the core is
    // consumed as a static library.  Its constructor attribute runs before main().
    (void)huge_page_reservation();
    static Impl value;
    return value;
}

void MatchingEngine::reset() {
    Impl& book = state();
    book.orders.clear();
    book.bids.clear();
    book.asks.clear();
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

ProcessResult MatchingEngine::handle_add_request(const AddOrderRequest& add) {
    ProcessResult result;

    if (add.order_id == 0U) {
        result.errors.emplace_back("Invalid AddOrderRequest orderid");
        return result;
    }

    if (add.quantity == 0U) {
        result.errors.emplace_back("Invalid AddOrderRequest quantity");
        return result;
    }

    if (!std::isfinite(add.price) || add.price <= 0.0L) {
        result.errors.emplace_back("Invalid AddOrderRequest price");
        return result;
    }

    Impl& book = state();
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

ProcessResult MatchingEngine::handle_cancel_request(const CancelOrderRequest& cancel) {
    ProcessResult result;

    if (cancel.order_id == 0U) {
        result.errors.emplace_back("Invalid CancelOrderRequest orderid");
        return result;
    }

    Impl& book = state();
    auto order_it = book.orders.find(cancel.order_id);
    if (order_it == book.orders.end()) {
        result.errors.emplace_back("Unknown orderid for cancel: " + std::to_string(cancel.order_id));
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
