#include <matching_engine_core/add_order_request.hpp>
#include <matching_engine_core/cancel_order_request.hpp>
#include <matching_engine_core/matching_engine.hpp>
#include <matching_engine_core/side.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>

namespace {

std::atomic<bool> track_heap_allocations{false};
std::atomic<std::size_t> heap_allocation_count{0U};

void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
    if (track_heap_allocations.load(std::memory_order_relaxed)) {
        heap_allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }

    void* memory = nullptr;
    if (alignment <= alignof(std::max_align_t)) {
        memory = std::malloc(size == 0U ? 1U : size);
    } else if (posix_memalign(&memory, alignment, size == 0U ? alignment : size) != 0) {
        memory = nullptr;
    }
    if (memory == nullptr) {
        throw std::bad_alloc{};
    }
    return memory;
}

void deallocate(void* memory) noexcept {
    std::free(memory);
}

constexpr std::size_t kIterations = 100'000U;

[[nodiscard]] bool run_matching_cycle(matching_engine::MatchingEngine& engine,
                                      std::uint64_t order_id) {
    const auto resting = engine.handle_add_request(
        matching_engine::AddOrderRequest{order_id, matching_engine::Side::Buy, 10U, 100.0L});
    if (!resting.errors.empty()) {
        return false;
    }

    const auto aggressive = engine.handle_add_request(matching_engine::AddOrderRequest{
        order_id + 1U, matching_engine::Side::Sell, 5U, 99.0L});
    if (!aggressive.errors.empty() || aggressive.outputs.size() != 3U) {
        return false;
    }

    const auto cancelled =
        engine.handle_cancel_request(matching_engine::CancelOrderRequest{order_id});
    return cancelled.errors.empty();
}

[[nodiscard]] std::uint64_t percentile(std::array<std::uint64_t, kIterations> samples,
                                        std::size_t numerator) {
    std::sort(samples.begin(), samples.end());
    const std::size_t index = (samples.size() - 1U) * numerator / 100U;
    return samples[index];
}

}  // namespace

void* operator new(std::size_t size) {
    return allocate(size);
}

void* operator new[](std::size_t size) {
    return allocate(size);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocate(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocate(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* memory) noexcept {
    deallocate(memory);
}

void operator delete[](void* memory) noexcept {
    deallocate(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    deallocate(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    deallocate(memory);
}

void operator delete(void* memory, std::align_val_t) noexcept {
    deallocate(memory);
}

void operator delete[](void* memory, std::align_val_t) noexcept {
    deallocate(memory);
}

void operator delete(void* memory, std::size_t, std::align_val_t) noexcept {
    deallocate(memory);
}

void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept {
    deallocate(memory);
}

int main() {
    matching_engine::MatchingEngine engine;
    engine.reset();

    // Initialize function-local state and populate every fixed-pool container
    // before allocation accounting begins.
    if (!run_matching_cycle(engine, 1U)) {
        std::cerr << "Warm-up flow failed\n";
        return 1;
    }

    std::array<std::uint64_t, kIterations> samples{};
    heap_allocation_count.store(0U, std::memory_order_relaxed);
    track_heap_allocations.store(true, std::memory_order_relaxed);

    bool flow_succeeded = true;
    for (std::size_t index = 0U; index < kIterations; ++index) {
        const auto start = std::chrono::steady_clock::now();
        flow_succeeded = run_matching_cycle(engine, 10U + index * 2U) && flow_succeeded;
        const auto end = std::chrono::steady_clock::now();
        samples[index] = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    track_heap_allocations.store(false, std::memory_order_relaxed);
    const std::size_t allocations = heap_allocation_count.load(std::memory_order_relaxed);
    if (!flow_succeeded) {
        std::cerr << "Benchmark flow produced an unexpected result\n";
        return 1;
    }

    const std::uint64_t p50 = percentile(samples, 50U);
    const std::uint64_t p99 = percentile(samples, 99U);
    std::cout << "Matching cycle latency (add + match + cancel), " << kIterations
              << " iterations\n"
              << "p50: " << p50 << " ns\n"
              << "p99: " << p99 << " ns\n"
              << "Heap allocations during measured loop: " << allocations << '\n';

    return allocations == 0U ? 0 : 2;
}
