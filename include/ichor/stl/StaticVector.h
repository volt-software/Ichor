#pragma once

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <initializer_list>
#include <iterator>
#include <array>
#include <limits>
#include <memory>
#include <utility>
#include <algorithm>
#include <exception>
#include <functional>
#include <ichor/stl/CompilerSpecific.h>

// An implementation that is close to the StaticVector proposal: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p0843r6.html
// Difference being that this implementation also supports type-erasure
// Shoutout to David Stone, more info at https://www.youtube.com/watch?v=I8QJLGI0GOE
namespace Ichor::v1 {

    namespace Detail {
        template <typename T>
        inline constexpr auto is_sufficiently_trivial = std::is_trivially_default_constructible_v<T> && std::is_trivially_destructible_v<T>;

        template <typename T, std::size_t N>
        struct AlignedByteArray {
            alignas(T) std::array<std::byte, N> buff;
        };
        template<typename T>
        struct AlignedByteArray<T, 0> {
        };


        template <typename T, std::size_t N>
        struct UninitializedArray {
            constexpr auto data() noexcept ICHOR_LIFETIME_BOUND -> T * {
                if constexpr (is_sufficiently_trivial<T>) {
                    return static_cast<T *>(_storage.data());
                } else {
                    return reinterpret_cast<T *>(_storage.buff.data());
                }
            }
            [[nodiscard]] constexpr auto data() const noexcept ICHOR_LIFETIME_BOUND -> T const * {
                if constexpr (is_sufficiently_trivial<T>) {
                    return static_cast<T const *>(_storage.data());
                } else {
                    return reinterpret_cast<T const *>(_storage.buff.data());
                }
            }

        private:
            using storage_type = std::conditional_t<is_sufficiently_trivial<T>, std::array<T, N>, AlignedByteArray<T, sizeof(T) * N>>;
            [[no_unique_address]] storage_type _storage;
        };
        template<typename T>
        struct UninitializedArray<T, 0> {
            constexpr auto data() noexcept ICHOR_LIFETIME_BOUND -> T * {
                return nullptr;
            }
            [[nodiscard]] constexpr auto data() const noexcept ICHOR_LIFETIME_BOUND -> T const * {
                return nullptr;
            }
        };

        template <std::size_t N>
        struct SizeType {
            using type = std::conditional_t<N <= std::numeric_limits<unsigned char>::max(), unsigned char, std::conditional_t<N <= std::numeric_limits<unsigned short>::max(), unsigned short, std::conditional_t<N <= std::numeric_limits<unsigned int>::max(), unsigned int, std::size_t>>>;
        };

        // Moves if T supports moving, copies otherwise. Used when shifting elements around, so that copy-only types keep working.
        template <typename T>
        [[nodiscard]] constexpr decltype(auto) moveOrCopy(T &t) noexcept {
            if constexpr (std::is_move_constructible_v<T> && std::is_move_assignable_v<T>) {
                return std::move(t);
            } else {
                return static_cast<T const &>(t);
            }
        }
    }

    template <typename T, bool Const>
    struct SVIterator {
    public:
        using iterator_category = std::contiguous_iterator_tag;
        using iterator_concept  = std::contiguous_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using element_type      = std::conditional_t<Const, T const, T>;
        using pointer           = std::conditional_t<Const, T const *, T *>;
        using reference         = std::conditional_t<Const, value_type const &, value_type &>;

        constexpr SVIterator() noexcept : _ptr() {}
        constexpr explicit SVIterator(pointer ptr ICHOR_LIFETIME_BOUND) noexcept : _ptr(ptr) {}
        // iterator -> const_iterator conversion, like every std container offers
        template <bool OtherConst> requires (Const && !OtherConst)
        constexpr SVIterator(SVIterator<T, OtherConst> const &o) noexcept : _ptr(o.operator->()) {}

        constexpr reference operator*() const noexcept ICHOR_LIFETIME_BOUND { return *_ptr; }
        constexpr pointer operator->() const noexcept ICHOR_LIFETIME_BOUND { return _ptr; }
        constexpr reference operator[](difference_type n) const noexcept ICHOR_LIFETIME_BOUND { return _ptr[n]; }

        constexpr SVIterator& operator++() noexcept { ++_ptr; return *this; }
        constexpr SVIterator operator++(int) noexcept { return SVIterator{_ptr++}; }
        constexpr SVIterator& operator+=(difference_type n) noexcept { _ptr += n; return *this; }

        constexpr SVIterator& operator--() noexcept { --_ptr; return *this; }
        constexpr SVIterator operator--(int) noexcept { return SVIterator{_ptr--}; }
        constexpr SVIterator& operator-=(difference_type n) noexcept { _ptr -= n; return *this; }

        [[nodiscard]] friend constexpr bool operator== (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr == b._ptr; };
        [[nodiscard]] friend constexpr bool operator!= (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr != b._ptr; };
        [[nodiscard]] friend constexpr bool operator<  (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr <  b._ptr; };
        [[nodiscard]] friend constexpr bool operator<= (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr <= b._ptr; };
        [[nodiscard]] friend constexpr bool operator>  (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr >  b._ptr; };
        [[nodiscard]] friend constexpr bool operator>= (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr >= b._ptr; };
        [[nodiscard]] friend constexpr difference_type operator- (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr - b._ptr; };
        [[nodiscard]] friend constexpr SVIterator operator- (SVIterator const& a, difference_type b) noexcept { return SVIterator{ a._ptr - b }; };
        [[nodiscard]] friend constexpr SVIterator operator+ (SVIterator const& a, difference_type b) noexcept { return SVIterator{ a._ptr + b }; };
        [[nodiscard]] friend constexpr SVIterator operator+ (difference_type a, SVIterator const& b) noexcept { return SVIterator{ a + b._ptr }; };

    private:
        pointer _ptr;
    };

    template <typename T>
    class IStaticVector {
    public:
        using value_type = T;
        using pointer = T*;
        using const_pointer = T const *;
        using reference = value_type&;
        using const_reference = value_type const &;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using iterator = SVIterator<T, false>;
        using const_iterator = SVIterator<T, true>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        virtual constexpr void assign(size_type n, const_reference u) = 0;

        virtual constexpr iterator               begin()         noexcept ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_iterator         begin()   const noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr iterator               end()           noexcept ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_iterator         end()     const noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr reverse_iterator       rbegin()        noexcept ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_reverse_iterator rbegin()  const noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr reverse_iterator       rend()          noexcept ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_reverse_iterator rend()    const noexcept ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_iterator         cbegin()  const noexcept ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_iterator         cend()    const noexcept ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_reverse_iterator crbegin() const noexcept ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_reverse_iterator crend()   const noexcept ICHOR_LIFETIME_BOUND = 0;

        [[nodiscard]] virtual constexpr bool empty() const noexcept = 0;
        [[nodiscard]] virtual constexpr size_type size() const noexcept = 0;
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR virtual constexpr size_type max_size() const noexcept = 0;
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR virtual constexpr size_type capacity() const noexcept = 0;

        constexpr void resize(size_type sz) requires(std::is_default_constructible_v<T>) {
            assert(sz <= max_size());
            if(sz > size()) {
                size_type origSize = size();
                for(size_type i = origSize; i < sz; ++i) {
                    push_back(T());
                }
            } else if(sz < size()) {
                erase(cbegin() + static_cast<difference_type>(sz), cbegin() + static_cast<difference_type>(size()));
            }
        }

        virtual constexpr void resize(size_type sz, const_reference c) = 0;

        virtual constexpr reference       operator[](size_type n) ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr const_reference operator[](size_type n) const ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr reference       front() ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_reference front() const ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr reference       back() ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_reference back() const ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr pointer         data()       noexcept ICHOR_LIFETIME_BOUND = 0;
        [[nodiscard]] virtual constexpr const_pointer   data() const noexcept ICHOR_LIFETIME_BOUND = 0;

        virtual constexpr iterator insert(const_iterator const_position, const_reference x) ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr iterator insert(const_iterator const_position, value_type&& x) ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr iterator insert(const_iterator const_position, size_type n, const_reference x) ICHOR_LIFETIME_BOUND = 0;

        virtual constexpr void push_back(const_reference x) = 0;
        virtual constexpr void push_back(value_type&& x) = 0;

        virtual constexpr void pop_back() noexcept(Detail::is_sufficiently_trivial<T>) = 0;
        virtual constexpr iterator erase(const_iterator position) ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr iterator erase(const_iterator first, const_iterator last) ICHOR_LIFETIME_BOUND = 0;

        virtual constexpr void clear() noexcept = 0;

        [[nodiscard]] friend constexpr bool operator== (IStaticVector<T> const & a, IStaticVector<T> const & b) noexcept { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), b.end()); };
        [[nodiscard]] friend constexpr bool operator!= (IStaticVector<T> const & a, IStaticVector<T> const & b) noexcept { return !(a == b); };
        [[nodiscard]] friend constexpr bool operator<  (IStaticVector<T> const & a, IStaticVector<T> const & b) noexcept { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); };
        [[nodiscard]] friend constexpr bool operator<= (IStaticVector<T> const & a, IStaticVector<T> const & b) noexcept { return !(b < a); };
        [[nodiscard]] friend constexpr bool operator>  (IStaticVector<T> const & a, IStaticVector<T> const & b) noexcept { return   b < a; };
        [[nodiscard]] friend constexpr bool operator>= (IStaticVector<T> const & a, IStaticVector<T> const & b) noexcept { return !(a < b); };

    protected:
        ~IStaticVector() = default;
    };

    template <typename T, std::size_t N>
    class StaticVector final : public IStaticVector<T> {
    public:
        using value_type = T;
        using pointer = T*;
        using const_pointer = T const *;
        using reference = value_type&;
        using const_reference = value_type const &;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using storage_size_type = typename Detail::SizeType<N>::type;
        using iterator = SVIterator<T, false>;
        using const_iterator = SVIterator<T, true>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using IStaticVector<T>::resize;

        constexpr StaticVector() noexcept = default;
        constexpr explicit StaticVector(size_type n) noexcept(Detail::is_sufficiently_trivial<T>) {
            assert(n <= N);
            for(size_type i = 0; i < n; ++i) {
                emplace_back();
            }
        }
        constexpr StaticVector(size_type n, const_reference value) {
            assert(n <= N);
            for(size_type i = 0; i < n; ++i) {
                push_back(value);
            }
        }
        template <std::input_iterator InputIterator>
        constexpr StaticVector(InputIterator first, InputIterator last) {
            while(first != last) {
                push_back(*first);
                ++first;
            }
        }
        constexpr StaticVector(std::initializer_list<value_type> list) {
            assert(list.size() <= N);
            if constexpr (Detail::is_sufficiently_trivial<T>) {
                std::copy(list.begin(), list.end(), begin());
                _size = static_cast<storage_size_type>(list.size());
            } else {
                for(auto const &t : list) {
                    push_back(t);
                }
            }
        }
        constexpr StaticVector(StaticVector const & other) noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
            assert(other._size <= N);
            for(auto const &t : other) {
                push_back(t);
            }
        }
        constexpr StaticVector(StaticVector&& other) noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            assert(other._size <= N);
            for(auto &t : other) {
                emplace_back(Detail::moveOrCopy(t));
            }
            other.clear();
        }

        constexpr StaticVector& operator=(StaticVector const & other) noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
            if(this == &other) [[unlikely]] {
                return *this;
            }
            clear();
            for(auto const &t : other) {
                push_back(t);
            }
            return *this;
        }
        constexpr StaticVector& operator=(StaticVector&& other) noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            if(this == &other) [[unlikely]] {
                return *this;
            }
            clear();
            for(auto &t : other) {
                emplace_back(Detail::moveOrCopy(t));
            }
            other.clear();
            return *this;
        }

        /// Replaces the contents with [first, last). Like std::vector, the range must not point into this vector.
        template <std::input_iterator InputIterator>
        constexpr void assign(InputIterator first, InputIterator last) {
            size_type i = 0;
            // re-use already constructed elements, then either construct the remainder or destroy the leftovers
            while(first != last && i < _size) {
                _data.data()[i] = *first;
                ++i;
                ++first;
            }
            if(first == last) {
                truncate(i);
                return;
            }
            while(first != last) {
                push_back(*first);
                ++first;
            }
        }
        constexpr void assign(size_type n, const_reference u) final {
            assert(n <= N);
            // u may refer to an element of this vector, so first overwrite, then grow and only shrink at the very end.
            size_type const overlap = std::min(n, static_cast<size_type>(_size));
            for(size_type i = 0; i < overlap; ++i) {
                _data.data()[i] = u;
            }
            while(_size < n) {
                push_back(u);
            }
            truncate(n);
        }

        constexpr ~StaticVector() = default;

        constexpr ~StaticVector() requires (!Detail::is_sufficiently_trivial<T>) {
            clear();
        }

        constexpr iterator               begin()         noexcept ICHOR_LIFETIME_BOUND final {
            return iterator{_data.data()};
        }
        [[nodiscard]] constexpr const_iterator         begin()   const noexcept ICHOR_LIFETIME_BOUND final {
            return const_iterator{_data.data()};
        }
        constexpr iterator               end()           noexcept ICHOR_LIFETIME_BOUND final {
            return iterator{_data.data() + _size};
        }
        [[nodiscard]] constexpr const_iterator         end()     const noexcept ICHOR_LIFETIME_BOUND final {
            return const_iterator{_data.data() + _size};
        }
        constexpr reverse_iterator       rbegin()        noexcept ICHOR_LIFETIME_BOUND final {
            return reverse_iterator{end()};
        }
        [[nodiscard]] constexpr const_reverse_iterator rbegin()  const noexcept ICHOR_LIFETIME_BOUND final {
            return const_reverse_iterator{end()};
        }
        constexpr reverse_iterator       rend()          noexcept ICHOR_LIFETIME_BOUND final {
            return reverse_iterator{begin()};
        }
        [[nodiscard]] constexpr const_reverse_iterator rend()    const noexcept ICHOR_LIFETIME_BOUND final {
            return const_reverse_iterator{begin()};
        }
        [[nodiscard]] constexpr const_iterator         cbegin()  const noexcept ICHOR_LIFETIME_BOUND final {
            return const_iterator{_data.data()};
        }
        [[nodiscard]] constexpr const_iterator         cend()    const noexcept ICHOR_LIFETIME_BOUND final {
            return const_iterator{_data.data() + _size};
        }
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept ICHOR_LIFETIME_BOUND final {
            return const_reverse_iterator{cend()};
        }
        [[nodiscard]] constexpr const_reverse_iterator crend()   const noexcept ICHOR_LIFETIME_BOUND final {
            return const_reverse_iterator{cbegin()};
        }

        [[nodiscard]] constexpr bool empty() const noexcept final {
            return _size == 0;
        }
        [[nodiscard]] constexpr size_type size() const noexcept final {
            return _size;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr size_type max_size() const noexcept final {
            return N;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr size_type capacity() const noexcept final {
            return N;
        }

        constexpr void resize(size_type sz, const_reference c) final {
            assert(sz <= N);
            // c may refer to an element of this vector, growing never invalidates it and shrinking doesn't read it.
            while(_size < sz) {
                push_back(c);
            }
            truncate(sz);
        }

        constexpr reference       operator[](size_type n) ICHOR_LIFETIME_BOUND final {
            assert(n < _size);
            return *(_data.data() + n);
        }
        constexpr const_reference operator[](size_type n) const ICHOR_LIFETIME_BOUND final {
            assert(n < _size);
            return *(_data.data() + n);
        }
        constexpr reference       front() ICHOR_LIFETIME_BOUND final {
            assert(_size > 0);
            return *_data.data();
        }
        [[nodiscard]] constexpr const_reference front() const ICHOR_LIFETIME_BOUND final {
            assert(_size > 0);
            return *_data.data();
        }
        constexpr reference       back() ICHOR_LIFETIME_BOUND final {
            assert(_size > 0);
            return *(_data.data() + (_size - 1));
        }
        [[nodiscard]] constexpr const_reference back() const ICHOR_LIFETIME_BOUND final {
            assert(_size > 0);
            return *(_data.data() + (_size - 1));
        }
        constexpr pointer         data()       noexcept ICHOR_LIFETIME_BOUND final {
            return _data.data();
        }
        [[nodiscard]] constexpr const_pointer   data() const noexcept ICHOR_LIFETIME_BOUND final {
            return _data.data();
        }

        constexpr iterator insert(const_iterator const_position, const_reference x) ICHOR_LIFETIME_BOUND final {
            return insert(const_position, 1, x);
        }

        constexpr iterator insert(const_iterator const_position, value_type&& x) ICHOR_LIFETIME_BOUND final {
            if constexpr (std::is_move_constructible_v<T>) {
                if(isElement(std::addressof(x))) {
                    T tmp(std::move(x));
                    return insertWith(const_position, 1, [&tmp](T *slot, bool alive, size_type) { fillSlot(slot, alive, std::move(tmp)); });
                }
                return insertWith(const_position, 1, [&x](T *slot, bool alive, size_type) { fillSlot(slot, alive, std::move(x)); });
            } else {
                std::terminate(); // should never be called because value_type&& is impossible for non-moveable types
            }
        }

        constexpr iterator insert(const_iterator const_position, size_type n, const_reference x) ICHOR_LIFETIME_BOUND final {
            if(isElement(std::addressof(x))) {
                // x would be moved around while making space for it, so take a copy first.
                T const tmp(x);
                return insertWith(const_position, n, [&tmp](T *slot, bool alive, size_type) { fillSlot(slot, alive, tmp); });
            }
            return insertWith(const_position, n, [&x](T *slot, bool alive, size_type) { fillSlot(slot, alive, x); });
        }

        /// Inserts [first, last) before const_position. Like std::vector, the range must not point into this vector.
        template <std::input_iterator InputIterator>
        constexpr iterator insert(const_iterator const_position, InputIterator first, InputIterator last) ICHOR_LIFETIME_BOUND {
            if constexpr (std::forward_iterator<InputIterator>) {
                auto const n = static_cast<size_type>(std::distance(first, last));
                return insertWith(const_position, n, [&first](T *slot, bool alive, size_type) {
                    fillSlot(slot, alive, *first);
                    ++first;
                });
            } else {
                // single-pass iterators: append everything and rotate it into place
                auto const idx = static_cast<size_type>(const_position - cbegin());
                auto const origSize = static_cast<difference_type>(_size);
                while(first != last) {
                    push_back(*first);
                    ++first;
                }
                std::rotate(begin() + static_cast<difference_type>(idx), begin() + origSize, end());
                return begin() + static_cast<difference_type>(idx);
            }
        }

        template <class... Args>
        constexpr iterator emplace(const_iterator const_position, Args&&... args) ICHOR_LIFETIME_BOUND {
            assert(_size < N);
            if(const_position == cend()) {
                emplace_back(std::forward<Args>(args)...);
                return end() - 1;
            }
            // args may refer to elements of this vector, construct the value before shifting anything.
            T tmp(std::forward<Args>(args)...);
            return insertWith(const_position, 1, [&tmp](T *slot, bool alive, size_type) { fillSlot(slot, alive, Detail::moveOrCopy(tmp)); });
        }
        template <class... Args>
        constexpr reference emplace_back(Args&&... args) noexcept(Detail::is_sufficiently_trivial<T>) ICHOR_LIFETIME_BOUND {
            assert(_size < N);
            std::construct_at(data() + size(), std::forward<Args>(args)...);
            ++_size;
            return back();
        }
        constexpr void push_back(const_reference x) final {
            assert(_size < N);
            std::construct_at(data() + size(), x);
            ++_size;
        }
        constexpr void push_back(value_type&& x) final {
            assert(_size < N);
            if constexpr (std::is_move_constructible_v<T>) {
                std::construct_at(data() + size(), std::move(x));
            } else {
                std::terminate(); // should never be called because value_type&& is impossible for non-moveable types
            }
            ++_size;
        }

        constexpr void pop_back() noexcept(Detail::is_sufficiently_trivial<T>) final {
            assert(_size > 0);
            if constexpr (!Detail::is_sufficiently_trivial<T>) {
                std::destroy_at(std::addressof(back()));
            }
            --_size;
        }
        constexpr iterator erase(const_iterator const_position) ICHOR_LIFETIME_BOUND final {
            assert(const_position >= cbegin() && const_position < cend());
            auto position = iterator{const_cast<pointer>(const_position.operator->())};
            if(position + 1 != end()) {
                if constexpr (std::is_move_assignable_v<T>) {
                    std::move(position + 1, end(), position);
                } else {
                    std::copy(position + 1, end(), position);
                }
            }
            pop_back();
            return position;
        }
        constexpr iterator erase(const_iterator const_first, const_iterator const_last) ICHOR_LIFETIME_BOUND final {
            auto first = iterator{const_cast<pointer>(const_first.operator->())};
            if(const_first != const_last) {
                auto last = iterator{const_cast<pointer>(const_last.operator->())};
                iterator position;

                if constexpr (std::is_move_assignable_v<T>) {
                    position = std::move(last, end(), first);
                } else {
                    position = std::copy(last, end(), first);
                }

                truncate(static_cast<size_type>(position - begin()));
            }

            return first;
        }

        constexpr void clear() noexcept final {
            truncate(0);
        }

        constexpr void swap(StaticVector &x) noexcept(std::is_nothrow_swappable_v<T> && std::is_nothrow_move_constructible_v<T>) {
            static_assert(std::is_swappable_v<T>, "T is not swappable, implement swap on the type");
            if(this == &x) [[unlikely]] {
                return;
            }

            StaticVector &smaller = _size <= x._size ? *this : x;
            StaticVector &larger = _size <= x._size ? x : *this;
            auto const common = static_cast<size_type>(smaller._size);

            for(size_type i = 0; i < common; ++i) {
                using std::swap;
                swap(smaller._data.data()[i], larger._data.data()[i]);
            }

            // the remainder only exists in the larger vector, move it over into raw storage of the smaller one
            for(size_type i = common; i < larger._size; ++i) {
                smaller.emplace_back(Detail::moveOrCopy(larger._data.data()[i]));
            }
            larger.truncate(common);
        }

        friend constexpr void swap(StaticVector &a, StaticVector &b) noexcept(noexcept(a.swap(b))) {
            a.swap(b);
        }

    private:
        /// Destroys all elements from newSize onwards
        constexpr void truncate(size_type newSize) noexcept {
            if constexpr (!Detail::is_sufficiently_trivial<T>) {
                for(size_type i = newSize; i < _size; ++i) {
                    std::destroy_at(_data.data() + i);
                }
            }
            if(newSize < _size) {
                _size = static_cast<storage_size_type>(newSize);
            }
        }

        /// Whether p points to one of the live elements of this vector.
        [[nodiscard]] constexpr bool isElement(T const *p) const noexcept {
            if (std::is_constant_evaluated()) {
                // comparing unrelated pointers is not a constant expression, so walk the elements instead.
                for(size_type i = 0; i < _size; ++i) {
                    if(_data.data() + i == p) {
                        return true;
                    }
                }
                return false;
            }
            return std::less_equal<T const *>{}(_data.data(), p) && std::less<T const *>{}(p, _data.data() + _size);
        }

        /// Puts a value into a slot that either contains a live (moved-from) object or raw storage.
        template <typename U>
        static constexpr void fillSlot(T *slot, bool alive, U &&value) {
            if(alive) {
                *slot = std::forward<U>(value);
            } else {
                std::construct_at(slot, std::forward<U>(value));
            }
        }

        /// Opens a gap of n elements before const_position and calls fill(slot, alive, index) for each slot of the gap.
        template <typename Fill>
        constexpr iterator insertWith(const_iterator const_position, size_type n, Fill &&fill) {
            assert(_size + n <= N);
            assert(const_position >= cbegin() && const_position <= cend());
            auto const idx = static_cast<size_type>(const_position - cbegin());
            if(n == 0) {
                // nothing to insert, shifting would self-move-assign every element after idx
                return begin() + static_cast<difference_type>(idx);
            }
            auto const origSize = static_cast<size_type>(_size);
            T *d = _data.data();

            // Shift [idx, origSize) right by n, back to front. Destinations beyond the old end are raw storage and need constructing.
            for(size_type i = origSize; i > idx; --i) {
                size_type const src = i - 1;
                size_type const dst = src + n;
                if(dst >= origSize) {
                    std::construct_at(d + dst, Detail::moveOrCopy(d[src]));
                } else {
                    d[dst] = Detail::moveOrCopy(d[src]);
                }
            }

            for(size_type k = 0; k < n; ++k) {
                fill(d + idx + k, idx + k < origSize, k);
            }

            _size = static_cast<storage_size_type>(origSize + n);
            return begin() + static_cast<difference_type>(idx);
        }

        Detail::UninitializedArray<T, N> _data;
        storage_size_type _size{};
    };
}
