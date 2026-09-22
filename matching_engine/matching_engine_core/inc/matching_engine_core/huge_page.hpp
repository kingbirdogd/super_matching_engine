#pragma once

#include <cstddef>

namespace matching_engine {

/// The outcome of the core library's process-startup HugeTLB reservation.
///
/// The reservation is attempted before main() on Linux.  It succeeds only
/// when the host has a compatible explicit huge page already reserved.
struct HugePageReservation {
    bool is_reserved{false};
    std::size_t size_bytes{0U};
};

/// Returns the status of the HugeTLB reservation attempted by this library.
///
/// The default reservation is 500 MiB.  Set MATCHING_ENGINE_HUGE_PAGE_SIZE_MB
/// to a positive integer before process startup to override that size.
[[nodiscard]] HugePageReservation huge_page_reservation() noexcept;

}  // namespace matching_engine
