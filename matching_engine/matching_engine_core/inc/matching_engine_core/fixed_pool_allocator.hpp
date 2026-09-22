#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>

namespace matching_engine {

/// A fixed-capacity allocator for STL node-based containers.
///
/// Storage is one contiguous, statically allocated array of slots.  Every
/// allocation and deallocation must be for exactly one object.  Consequently,
/// this allocator is suitable for containers such as std::list, std::map, and
/// std::unordered_map, but not std::vector (which requests blocks of objects
/// when it grows).  The pool is shared by all allocator instances with the
/// same T and Capacity and is not thread-safe.
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
        if (count != 1U || pool_.available == 0U) {
            throw std::bad_alloc{};
        }

        const size_type slot_index = pool_.free_slots[--pool_.available];
        assert(!pool_.in_use[slot_index]);
        pool_.in_use[slot_index] = true;
        return pool_.pointer_at(slot_index);
    }

    void deallocate(T* pointer, size_type count) noexcept {
        assert(count == 1U);
        pool_.release(pointer);
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
        std::array<size_type, Capacity> free_slots{};
        std::array<bool, Capacity> in_use{};
        size_type available{Capacity};

        Pool() noexcept {
            for (size_type index = 0U; index < Capacity; ++index) {
                free_slots[index] = index;
            }
        }

        [[nodiscard]] T* pointer_at(size_type index) noexcept {
            return std::launder(reinterpret_cast<T*>(slots[index].bytes));
        }

        void release(T* pointer) noexcept {
            for (size_type index = 0U; index < Capacity; ++index) {
                if (pointer == pointer_at(index)) {
                    assert(in_use[index]);
                    in_use[index] = false;
                    free_slots[available++] = index;
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
