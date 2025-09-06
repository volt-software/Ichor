#pragma once

#include <ichor/stl/NeverAlwaysNull.h>
#include <vector>
#include <type_traits>

namespace Ichor::v1 {
    // Span-like, non-allocating view over a vector.
    // Safe for vectors of any type derived from T (avoids base-pointer
    // arithmetic across derived arrays by indexing via the original element type).
    template <typename VecT>
    struct VectorView final {
        using value_type = NeverNull<const VecT*>;

        // Function pointer used to retrieve an element as VecT* from erased storage
        using GetFn = const VecT* (*)(const void* data, std::size_t idx) noexcept;

        struct iterator {
            using value_type = NeverNull<const VecT*>;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::random_access_iterator_tag;

            constexpr iterator() noexcept : _data(nullptr), _get(nullptr), _idx(0) {}
            constexpr iterator(const void* data, GetFn get, std::size_t idx) noexcept
                    : _data(data), _get(get), _idx(idx) {}

            constexpr value_type operator*() const noexcept {
                return value_type{_get(_data, _idx)};
            }
            constexpr iterator& operator++() noexcept { ++_idx; return *this; }
            constexpr iterator operator++(int) noexcept { auto tmp = *this; ++_idx; return tmp; }
            constexpr iterator& operator--() noexcept { --_idx; return *this; }
            constexpr iterator operator--(int) noexcept { auto tmp = *this; --_idx; return tmp; }

            friend constexpr bool operator==(const iterator& a, const iterator& b) noexcept { return a._idx == b._idx && a._data == b._data; }
            friend constexpr bool operator!=(const iterator& a, const iterator& b) noexcept { return !(a == b); }
            friend constexpr bool operator<(const iterator& a, const iterator& b) noexcept { return a._idx < b._idx; }
            friend constexpr bool operator>(const iterator& a, const iterator& b) noexcept { return b < a; }
            friend constexpr bool operator<=(const iterator& a, const iterator& b) noexcept { return !(b < a); }
            friend constexpr bool operator>=(const iterator& a, const iterator& b) noexcept { return !(a < b); }
            friend constexpr difference_type operator-(const iterator& a, const iterator& b) noexcept { return static_cast<difference_type>(a._idx) - static_cast<difference_type>(b._idx); }
            constexpr iterator operator+(difference_type n) const noexcept { return iterator{_data, _get, static_cast<std::size_t>(_idx + n)}; }
            constexpr iterator operator-(difference_type n) const noexcept { return iterator{_data, _get, static_cast<std::size_t>(_idx - n)}; }
            constexpr iterator& operator+=(difference_type n) noexcept { _idx = static_cast<std::size_t>(_idx + n); return *this; }
            constexpr iterator& operator-=(difference_type n) noexcept { _idx = static_cast<std::size_t>(_idx - n); return *this; }

        private:
            const void* _data;
            GetFn _get;
            std::size_t _idx;
        };

        constexpr VectorView() noexcept : _data(nullptr), _get(nullptr), _size(0) {}

        template <typename T, typename = std::enable_if_t<std::is_base_of_v<VecT, T>>>
        explicit constexpr VectorView(const std::vector<T>& v) noexcept
                : _data(v.data()), _get(&VectorView::template getAt<T>), _size(v.size()) {}

        [[nodiscard]] constexpr iterator begin() const noexcept { return iterator{_data, _get, 0}; }
        [[nodiscard]] constexpr iterator end() const noexcept { return iterator{_data, _get, _size}; }

        [[nodiscard]] constexpr std::size_t size() const noexcept { return _size; }
        [[nodiscard]] constexpr bool empty() const noexcept { return _size == 0; }

        [[nodiscard]] constexpr value_type operator[](std::size_t idx) const noexcept { return value_type{_get(_data, idx)}; }

    private:
        template <typename T>
        static constexpr const VecT* getAt(const void* data, std::size_t idx) noexcept {
            return static_cast<const VecT*>(static_cast<const T*>(data) + idx);
        }

        const void* _data;
        GetFn _get;
        std::size_t _size;
    };
}