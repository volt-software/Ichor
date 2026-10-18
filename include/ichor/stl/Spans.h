#pragma once

#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/stl/CompilerSpecific.h>
#include <cstddef>
#include <iterator>
#include <memory>
#include <vector>
#include <type_traits>

namespace Ichor::v1 {
    // VectorView: span-like, non-allocating view over a vector.
    // Used in situations where you have a vector<CHILD> but want to return a span<PARENT>.
    // Safe for vectors of any type derived from T (avoids base-pointer
    // arithmetic across derived arrays by indexing via the original element type).
    template <typename VecT>
    struct VectorView final {
        using value_type = NeverNull<VecT const *>;

        // Function pointer used to retrieve an element as VecT* from erased storage
        using GetFn = VecT const * (*)(void const * data, std::size_t idx) noexcept;

        struct iterator {
            using value_type = NeverNull<VecT const *>;
            using difference_type = std::ptrdiff_t;
            // operator* returns by value (a proxy), which C++20 random access iterators allow but legacy (C++17) forward iterators don't.
            using iterator_concept = std::random_access_iterator_tag;
            using iterator_category = std::input_iterator_tag;

            constexpr iterator() noexcept : _data(nullptr), _get(nullptr), _idx(0) {}
            constexpr iterator(void const * data, GetFn get, std::size_t idx) noexcept
                    : _data(data), _get(get), _idx(idx) {}

            constexpr value_type operator*() const noexcept {
                return value_type{_get(_data, _idx)};
            }
            constexpr iterator& operator++() noexcept { ++_idx; return *this; }
            constexpr iterator operator++(int) noexcept { auto tmp = *this; ++_idx; return tmp; }
            constexpr iterator& operator--() noexcept { --_idx; return *this; }
            constexpr iterator operator--(int) noexcept { auto tmp = *this; --_idx; return tmp; }
            constexpr value_type operator[](difference_type n) const noexcept { return _get(_data, static_cast<std::size_t>(static_cast<difference_type>(_idx) + n)); }

            // Like std iterators, only iterators into the same view may be compared, so comparing the index is enough and consistent with operator<.
            friend constexpr bool operator==(iterator const& a, iterator const& b) noexcept { return a._idx == b._idx; }
            friend constexpr bool operator!=(iterator const& a, iterator const& b) noexcept { return !(a == b); }
            friend constexpr bool operator<(iterator const& a, iterator const& b) noexcept { return a._idx < b._idx; }
            friend constexpr bool operator>(iterator const& a, iterator const& b) noexcept { return b < a; }
            friend constexpr bool operator<=(iterator const& a, iterator const& b) noexcept { return !(b < a); }
            friend constexpr bool operator>=(iterator const& a, iterator const& b) noexcept { return !(a < b); }
            friend constexpr difference_type operator-(iterator const& a, iterator const& b) noexcept { return static_cast<difference_type>(a._idx) - static_cast<difference_type>(b._idx); }
            friend constexpr iterator operator+(difference_type n, iterator const& a) noexcept { return iterator{a._data, a._get, static_cast<std::size_t>(static_cast<difference_type>(a._idx) + n)}; }
            constexpr iterator operator+(difference_type n) const noexcept { return iterator{_data, _get, static_cast<std::size_t>(static_cast<difference_type>(_idx) + n)}; }
            constexpr iterator operator-(difference_type n) const noexcept { return iterator{_data, _get, static_cast<std::size_t>(static_cast<difference_type>(_idx) - n)}; }
            constexpr iterator& operator+=(difference_type n) noexcept { _idx = static_cast<std::size_t>(static_cast<difference_type>(_idx) + n); return *this; }
            constexpr iterator& operator-=(difference_type n) noexcept { _idx = static_cast<std::size_t>(static_cast<difference_type>(_idx) - n); return *this; }

        private:
            void const * _data;
            GetFn _get;
            std::size_t _idx;
        };

        constexpr VectorView() noexcept : _data(nullptr), _get(nullptr), _size(0) {}

        /// The view refers to v's storage, it is invalidated when v is destroyed or reallocates.
        template <typename T>
        explicit constexpr VectorView(std::vector<T> const &v ICHOR_LIFETIME_BOUND) noexcept
                requires (std::is_base_of_v<VecT, T>) : _data(v.data()), _get(&VectorView::template getAt<T>), _size(v.size()) {}
        // A view over a temporary would dangle immediately.
        template <typename T>
        explicit VectorView(std::vector<T> const &&v) requires (std::is_base_of_v<VecT, T>) = delete;

        /// View over the objects owned by a vector of unique_ptrs. Invalidated when v is destroyed or reallocates.
        template <typename T>
        explicit constexpr VectorView(std::vector<std::unique_ptr<T>> const &v ICHOR_LIFETIME_BOUND) noexcept
                requires (std::is_base_of_v<VecT, T>) : _data(v.data()), _get(&VectorView::template getAtUniquePtr<T>), _size(v.size()) {}
        template <typename T>
        explicit VectorView(std::vector<std::unique_ptr<T>> const &&v) requires (std::is_base_of_v<VecT, T>) = delete;

        [[nodiscard]] constexpr iterator begin() const noexcept { return iterator{_data, _get, 0}; }
        [[nodiscard]] constexpr iterator end() const noexcept { return iterator{_data, _get, _size}; }

        [[nodiscard]] constexpr std::size_t size() const noexcept { return _size; }
        [[nodiscard]] constexpr bool empty() const noexcept { return _size == 0; }

        [[nodiscard]] constexpr value_type operator[](std::size_t idx) const noexcept { return value_type{_get(_data, idx)}; }

    private:
        template <typename T>
        static constexpr VecT const * getAt(void const * data, std::size_t idx) noexcept {
            return static_cast<VecT const *>(static_cast<T const *>(data) + idx);
        }

        template <typename T>
        static constexpr VecT const * getAtUniquePtr(void const * data, std::size_t idx) noexcept {
            return static_cast<VecT const *>((static_cast<std::unique_ptr<T> const *>(data) + idx)->get());
        }

        void const * _data;
        GetFn _get;
        std::size_t _size;
    };
}