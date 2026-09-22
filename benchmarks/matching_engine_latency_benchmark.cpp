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
#include <iostream>
#include <new>

namespace {

std::atomic<bool> track_heap_allocations{false};
std::atomic<std::size_t> heap_allocation_count{0U};

void* allocate(
    std::size_t size,
    std::size_t alignment = alignof(std::max_align_t)) {

    if (track_heap_allocations.load(std::memory_order_relaxed)) {
        heap_allocation_count.fetch_add(
            1U,
            std::memory_order_relaxed);
    }

    void* memory = nullptr;

    if (alignment <= alignof(std::max_align_t)) {
        memory = std::malloc(size == 0U ? 1U : size);
    } else if (
        posix_memalign(
            &memory,
            alignment,
            size == 0U ? alignment : size) != 0) {

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

constexpr std::size_t kIterations = 10'000'000U;

/*
 * Latency histogram
 *
 * Range                         Resolution
 * ------------------------------------------------
 *   0      -    2047 ns          1 ns
 * 2048     -    8191 ns          8 ns
 * 8192     -   65535 ns         64 ns
 * 65536    -  524287 ns        512 ns
 * 524288   - 4194303 ns       4096 ns
 * >= 4194304 ns                overflow
 *
 * Total buckets:
 *
 * 2048
 * + 768
 * + 896
 * + 896
 * + 896
 * + 1 overflow
 *
 * ~= 5505 uint64_t
 * ~= 44 KB
 *
 * The histogram is deliberately small compared with the
 * original 8 GB raw-sample array.
 */

class LatencyHistogram {
public:
    static constexpr std::size_t kFineEnd = 2048U;
    static constexpr std::size_t kLevel1Count = 768U;
    static constexpr std::size_t kLevel2Count = 896U;
    static constexpr std::size_t kLevel3Count = 896U;
    static constexpr std::size_t kLevel4Count = 896U;

    static constexpr std::size_t kLevel1Start = kFineEnd;

    static constexpr std::size_t kLevel2Start =
        kLevel1Start + kLevel1Count;

    static constexpr std::size_t kLevel3Start =
        kLevel2Start + kLevel2Count;

    static constexpr std::size_t kLevel4Start =
        kLevel3Start + kLevel3Count;

    static constexpr std::size_t kOverflowIndex =
        kLevel4Start + kLevel4Count;

    static constexpr std::size_t kBucketCount =
        kOverflowIndex + 1U;

    using Storage =
        std::array<std::uint64_t, kBucketCount>;

    LatencyHistogram() noexcept {
        buckets_.fill(0U);
    }

    void record(std::uint64_t latency_ns) noexcept {

        if (latency_ns < kFineEnd) {
            ++buckets_[static_cast<std::size_t>(latency_ns)];
            return;
        }

        /*
         * 2048..8191
         *
         * 8 ns resolution.
         */
        if (latency_ns < 8192U) {
            const auto bucket =
                kLevel1Start +
                static_cast<std::size_t>(
                    (latency_ns - 2048U) >> 3U);

            ++buckets_[bucket];
            return;
        }

        /*
         * 8192..65535
         *
         * 64 ns resolution.
         */
        if (latency_ns < 65536U) {
            const auto bucket =
                kLevel2Start +
                static_cast<std::size_t>(
                    (latency_ns - 8192U) >> 6U);

            ++buckets_[bucket];
            return;
        }

        /*
         * 65536..524287
         *
         * 512 ns resolution.
         */
        if (latency_ns < 524288U) {
            const auto bucket =
                kLevel3Start +
                static_cast<std::size_t>(
                    (latency_ns - 65536U) >> 9U);

            ++buckets_[bucket];
            return;
        }

        /*
         * 524288..4194303
         *
         * 4096 ns resolution.
         */
        if (latency_ns < 4194304U) {
            const auto bucket =
                kLevel4Start +
                static_cast<std::size_t>(
                    (latency_ns - 524288U) >> 12U);

            ++buckets_[bucket];
            return;
        }

        ++buckets_[kOverflowIndex];
    }

    [[nodiscard]]
    std::uint64_t percentile(
        std::uint64_t total_samples,
        std::uint64_t percentile_value) const noexcept {

        if (total_samples == 0U) {
            return 0U;
        }

        /*
         * ceil(total * percentile / 100)
         *
         * Avoid total * percentile overflow.
         */
        const std::uint64_t target =
            total_samples / 100U *
                percentile_value +
            ((total_samples % 100U) *
                 percentile_value +
             99U) /
                100U;

        std::uint64_t cumulative = 0U;

        for (std::size_t i = 0U;
             i < kOverflowIndex;
             ++i) {

            cumulative += buckets_[i];

            if (cumulative >= target) {
                return bucket_upper_bound_ns(i);
            }
        }

        return 4194304U;
    }

    [[nodiscard]]
    std::uint64_t overflow_count() const noexcept {
        return buckets_[kOverflowIndex];
    }

private:
    [[nodiscard]]
    static std::uint64_t bucket_upper_bound_ns(
        std::size_t index) noexcept {

        /*
         * Level 0:
         *
         * exact 1 ns buckets.
         */
        if (index < kFineEnd) {
            return static_cast<std::uint64_t>(index);
        }

        /*
         * Level 1:
         *
         * 2048 + bucket * 8
         */
        if (index < kLevel2Start) {
            const auto bucket =
                index - kLevel1Start;

            return 2048U +
                   static_cast<std::uint64_t>(
                       bucket + 1U) *
                       8U -
                   1U;
        }

        /*
         * Level 2:
         *
         * 8192 + bucket * 64
         */
        if (index < kLevel3Start) {
            const auto bucket =
                index - kLevel2Start;

            return 8192U +
                   static_cast<std::uint64_t>(
                       bucket + 1U) *
                       64U -
                   1U;
        }

        /*
         * Level 3:
         *
         * 65536 + bucket * 512
         */
        if (index < kLevel4Start) {
            const auto bucket =
                index - kLevel3Start;

            return 65536U +
                   static_cast<std::uint64_t>(
                       bucket + 1U) *
                       512U -
                   1U;
        }

        /*
         * Level 4:
         *
         * 524288 + bucket * 4096
         */
        const auto bucket =
            index - kLevel4Start;

        return 524288U +
               static_cast<std::uint64_t>(
                   bucket + 1U) *
                   4096U -
               1U;
    }

    Storage buckets_{};
};

[[nodiscard]]
bool run_matching_cycle(
    matching_engine::MatchingEngine& engine,
    std::uint64_t order_id) {

    const auto resting =
        engine.handle_add_request(
            matching_engine::AddOrderRequest{
                order_id,
                matching_engine::Side::Buy,
                10U,
                100.0L});

    if (!resting.errors.empty()) {
        return false;
    }

    const auto aggressive =
        engine.handle_add_request(
            matching_engine::AddOrderRequest{
                order_id + 1U,
                matching_engine::Side::Sell,
                5U,
                99.0L});

    if (!aggressive.errors.empty() ||
        aggressive.outputs.size() != 3U) {
        return false;
    }

    const auto cancelled =
        engine.handle_cancel_request(
            matching_engine::CancelOrderRequest{
                order_id});

    return cancelled.errors.empty();
}

} // namespace


/*
 * Global allocation operators.
 */

void* operator new(std::size_t size) {
    return allocate(size);
}

void* operator new[](std::size_t size) {
    return allocate(size);
}

void* operator new(
    std::size_t size,
    std::align_val_t alignment) {

    return allocate(
        size,
        static_cast<std::size_t>(alignment));
}

void* operator new[](
    std::size_t size,
    std::align_val_t alignment) {

    return allocate(
        size,
        static_cast<std::size_t>(alignment));
}

void operator delete(void* memory) noexcept {
    deallocate(memory);
}

void operator delete[](void* memory) noexcept {
    deallocate(memory);
}

void operator delete(
    void* memory,
    std::size_t) noexcept {

    deallocate(memory);
}

void operator delete[](
    void* memory,
    std::size_t) noexcept {

    deallocate(memory);
}

void operator delete(
    void* memory,
    std::align_val_t) noexcept {

    deallocate(memory);
}

void operator delete[](
    void* memory,
    std::align_val_t) noexcept {

    deallocate(memory);
}

void operator delete(
    void* memory,
    std::size_t,
    std::align_val_t) noexcept {

    deallocate(memory);
}

void operator delete[](
    void* memory,
    std::size_t,
    std::align_val_t) noexcept {

    deallocate(memory);
}


int main() {

    matching_engine::MatchingEngine engine;

    engine.reset();

    /*
     * ------------------------------------------------------------
     * Warm-up
     * ------------------------------------------------------------
     *
     * Anything lazy-initialized by the matching engine happens
     * before allocation tracking begins.
     */

    if (!run_matching_cycle(engine, 1U)) {
        std::cerr
            << "Warm-up flow failed\n";

        return 1;
    }

    /*
     * Histogram is tiny (~44 KB).
     *
     * It lives entirely inside main's stack frame and does NOT
     * require heap allocation.
     */
    LatencyHistogram histogram;

    heap_allocation_count.store(
        0U,
        std::memory_order_relaxed);

    track_heap_allocations.store(
        true,
        std::memory_order_relaxed);

    /*
     * ------------------------------------------------------------
     * Measured loop
     * ------------------------------------------------------------
     */

    bool flow_succeeded = true;

    for (std::size_t index = 0U;
         index < kIterations;
         ++index) {

        const auto start =
            std::chrono::steady_clock::now();

        const bool success =
            run_matching_cycle(
                engine,
                10U + index * 2U);

        const auto end =
            std::chrono::steady_clock::now();

        flow_succeeded =
            success && flow_succeeded;

        const auto latency_ns =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<
                    std::chrono::nanoseconds>(
                    end - start)
                    .count());

        histogram.record(latency_ns);
    }

    /*
     * Stop allocation tracking BEFORE printing or calculating
     * anything else.
     */
    track_heap_allocations.store(
        false,
        std::memory_order_relaxed);

    const std::size_t allocations =
        heap_allocation_count.load(
            std::memory_order_relaxed);

    /*
     * ------------------------------------------------------------
     * Validate benchmark
     * ------------------------------------------------------------
     */

    if (!flow_succeeded) {
        std::cerr
            << "Benchmark flow produced an unexpected result\n";

        return 1;
    }

    /*
     * ------------------------------------------------------------
     * Statistics
     * ------------------------------------------------------------
     */

    const std::uint64_t p50 =
        histogram.percentile(
            kIterations,
            50U);

    const std::uint64_t p99 =
        histogram.percentile(
            kIterations,
            99U);

    const std::uint64_t overflow =
        histogram.overflow_count();

    /*
     * ------------------------------------------------------------
     * Output
     * ------------------------------------------------------------
     */

    std::cout
        << "Matching cycle latency "
           "(add + match + cancel), "
        << kIterations
        << " iterations\n"
        << "p50: "
        << p50
        << " ns\n"
        << "p99: "
        << p99
        << " ns\n"
        << "Latency >= 4194304 ns: "
        << overflow
        << '\n'
        << "Heap allocations during measured loop: "
        << allocations
        << '\n';

    return allocations == 0U ? 0 : 2;
}