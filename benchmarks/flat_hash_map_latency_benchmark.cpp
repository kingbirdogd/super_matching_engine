#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
#include <absl/container/flat_hash_map.h>
#pragma GCC diagnostic pop

#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using Key = std::uint64_t;
using Value = std::uint64_t;

constexpr std::size_t kInitialOrders = 100'000U;
constexpr std::size_t kOperations = 2'000'000U;

template <typename Map>
[[nodiscard]] double benchmark_order_index(std::string_view name) {
    Map orders;
    orders.reserve(kInitialOrders);
    for (std::size_t index = 0U; index < kInitialOrders; ++index) {
        orders.emplace(static_cast<Key>(index), static_cast<Value>(index));
    }

    std::vector<Key> keys;
    keys.reserve(kOperations);
    std::uint64_t random_state = 0x9E3779B97F4A7C15ULL;
    for (std::size_t index = 0U; index < kOperations; ++index) {
        random_state ^= random_state >> 12U;
        random_state ^= random_state << 25U;
        random_state ^= random_state >> 27U;
        keys.push_back((random_state * 0x2545F4914F6CDD1DULL) % kInitialOrders);
    }

    const auto start = std::chrono::steady_clock::now();
    for (const Key key : keys) {
        const auto found = orders.find(key);
        if (found == orders.end()) {
            std::cerr << "Benchmark setup error: key missing\n";
            std::terminate();
        }

        const Value value = found->second + 1U;
        orders.erase(found);
        orders.emplace(key, value);
    }
    const auto end = std::chrono::steady_clock::now();

    const double elapsed_nanoseconds =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    const double nanoseconds_per_operation = elapsed_nanoseconds / static_cast<double>(kOperations);
    std::cout << std::left << std::setw(27) << name << std::fixed << std::setprecision(2)
              << nanoseconds_per_operation << " ns/op\n";
    return nanoseconds_per_operation;
}

}  // namespace

int main() {
    std::cout << "Order-index stress benchmark: " << kInitialOrders << " live entries, "
              << kOperations << " find/erase/emplace operations\n";

    const double unordered_map_latency =
        benchmark_order_index<std::unordered_map<Key, Value>>("std::unordered_map");
    const double flat_hash_map_latency =
        benchmark_order_index<absl::flat_hash_map<Key, Value>>("absl::flat_hash_map");

    const double percent_change =
        (flat_hash_map_latency - unordered_map_latency) / unordered_map_latency * 100.0;
    std::cout << "absl::flat_hash_map is " << std::abs(percent_change) << "% "
              << (percent_change <= 0.0 ? "lower latency" : "higher latency")
              << " in this run.\n";
    return 0;
}
