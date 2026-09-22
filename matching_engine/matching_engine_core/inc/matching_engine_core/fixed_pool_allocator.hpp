#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>

namespace matching_engine {

/// A fixed-capacity allocator for STL node-based containers.
///
/// Storage is one contiguous, statically allocated array of slots.  It can
/// serve both individual nodes and contiguous blocks, so it is suitable for
/// node-based containers as well as std::vector.  The pool is shared by all
/// allocator instances with the same T and Capacity and is not thread-safe.
template <typename T, std::size_t Capacity>
class FixedPoolAllocator {
    static_assert(Capacity > 0U, "FixedPoolAllocator capacity must be positive");

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind {
        using other = FixedPoolAllocator<U, Capacity>;
    };

    FixedPoolAllocator() noexcept = default;

    template <typename U>
    FixedPoolAllocator(const FixedPoolAllocator<U, Capacity>&) noexcept {}

    [[nodiscard]] T* allocate(size_type count) {
        if (count == 0U || count > pool_.available) {
            throw std::bad_alloc{};
        }

        return pool_.acquire(count);
    }

    void deallocate(T* pointer, size_type count) noexcept {
        assert(count > 0U);
        pool_.release(pointer, count);
    }

    [[nodiscard]] constexpr bool operator==(const FixedPoolAllocator&) const noexcept {
        return true;
    }

private:
    struct Slot {
        alignas(T) std::byte bytes[sizeof(T)];
    };

    struct Pool {
        std::array<Slot, Capacity> slots{};
        std::array<bool, Capacity> in_use{};
        size_type available{Capacity};

        [[nodiscard]] T* acquire(size_type count) {
            size_type consecutive = 0U;
            for (size_type index = 0U; index < Capacity; ++index) {
                consecutive = in_use[index] ? 0U : consecutive + 1U;
                if (consecutive == count) {
                    const size_type first = index + 1U - count;
                    for (size_type slot = first; slot <= index; ++slot) {
                        in_use[slot] = true;
                    }
                    available -= count;
                    return pointer_at(first);
                }
            }

            throw std::bad_alloc{};
        }

        [[nodiscard]] T* pointer_at(size_type index) noexcept {
            return std::launder(reinterpret_cast<T*>(slots[index].bytes));
        }

        void release(T* pointer, size_type count) noexcept {
            for (size_type index = 0U; index < Capacity; ++index) {
                if (pointer == pointer_at(index)) {
                    assert(index + count <= Capacity);
                    for (size_type slot = index; slot < index + count; ++slot) {
                        assert(in_use[slot]);
                        in_use[slot] = false;
                    }
                    available += count;
                    return;
                }
            }

            assert(false && "FixedPoolAllocator received a pointer outside its pool");
        }
    };

    inline static Pool pool_{};
};

template <typename T, typename U, std::size_t Capacity>
[[nodiscard]] constexpr bool operator==(const FixedPoolAllocator<T, Capacity>&,
                                        const FixedPoolAllocator<U, Capacity>&) noexcept {
    return std::is_same_v<T, U>;
}

template <typename T, typename U, std::size_t Capacity>
[[nodiscard]] constexpr bool operator!=(const FixedPoolAllocator<T, Capacity>& left,
                                        const FixedPoolAllocator<U, Capacity>& right) noexcept {
    return !(left == right);
}

}  // namespace matching_engine
