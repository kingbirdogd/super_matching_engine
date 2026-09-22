#include <matching_engine_core/huge_page.hpp>

#include <charconv>
#include <cstdlib>
#include <limits>
#include <string_view>

#if defined(__linux__)
#include <sys/mman.h>
#endif

namespace matching_engine {

namespace {

constexpr std::size_t kMiB = 1024U * 1024U;
constexpr std::size_t kDefaultHugePageReservationBytes = 500U * kMiB;
constexpr std::string_view kHugePageSizeEnvironmentVariable =
    "MATCHING_ENGINE_HUGE_PAGE_SIZE_MB";
HugePageReservation reservation{};

[[nodiscard]] std::size_t configured_huge_page_size_bytes() noexcept {
    const char* const configured_value = std::getenv(kHugePageSizeEnvironmentVariable.data());
    if (configured_value == nullptr) {
        return kDefaultHugePageReservationBytes;
    }

    const std::string_view value{configured_value};
    std::size_t size_mebibytes = 0U;
    const auto [end, error] =
        std::from_chars(value.data(), value.data() + value.size(), size_mebibytes);
    if (error != std::errc{} || end != value.data() + value.size() || size_mebibytes == 0U ||
        size_mebibytes > std::numeric_limits<std::size_t>::max() / kMiB) {
        return kDefaultHugePageReservationBytes;
    }

    return size_mebibytes * kMiB;
}

#if defined(__linux__)
void* reservation_address = nullptr;
std::size_t reservation_size_bytes = 0U;

[[gnu::constructor]] void reserve_huge_page_before_main() noexcept {
    const std::size_t requested_size_bytes = configured_huge_page_size_bytes();
    void* const address = mmap(nullptr,
                               requested_size_bytes,
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE,
                               -1,
                               0);
    if (address == MAP_FAILED) {
        return;
    }

    reservation_address = address;
    reservation_size_bytes = requested_size_bytes;
    reservation = HugePageReservation{true, reservation_size_bytes};
}

[[gnu::destructor]] void release_huge_page_at_process_exit() noexcept {
    if (reservation_address != nullptr) {
        (void)munmap(reservation_address, reservation_size_bytes);
        reservation_address = nullptr;
        reservation_size_bytes = 0U;
    }
}
#endif

}  // namespace

HugePageReservation huge_page_reservation() noexcept {
    return reservation;
}

}  // namespace matching_engine
