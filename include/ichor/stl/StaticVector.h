#pragma once

#include <cstddef>
#include <type_traits>
#include <iterator>
#include <array>
#include <limits>
#include <memory>
#include <algorithm>
#include <ichor/stl/CompilerSpecific.h>

// An implementation that is close to the StaticVector proposal: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p0843r6.html
// Difference being that this implementation also supports type-erasure
// Shoutout to David Stone, more info at https://www.youtube.com/watch?v=I8QJLGI0GOE
namespace Ichor::v1 {

    namespace Detail {
        template <typename T>
        static constexpr auto is_sufficiently_trivial = std::is_trivially_default_constructible_v<T> && std::is_trivially_destructible_v<T>;

        template <typename T, std::size_t N>
        struct AlignedByteArray {
            alignas(T) std::byte buff[N];
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
                    return reinterpret_cast<T *>(_storage.buff);
                }
            }
            constexpr auto data() const noexcept ICHOR_LIFETIME_BOUND -> T const * {
                if constexpr (is_sufficiently_trivial<T>) {
                    return static_cast<T const *>(_storage.data());
                } else {
                    return reinterpret_cast<T const *>(_storage.buff);
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
            constexpr auto data() const noexcept ICHOR_LIFETIME_BOUND -> T const * {
                return nullptr;
            }
        };

        template <std::size_t N>
        struct SizeType {
            using type = std::conditional_t<N <= std::numeric_limits<unsigned char>::max(), unsigned char, std::conditional_t<N <= std::numeric_limits<unsigned short>::max(), unsigned short, std::conditional_t<N <= std::numeric_limits<unsigned int>::max(), unsigned int, size_t>>>;
        };
    }

    template <typename T, bool Const>
    struct SVIterator {
    public:
        using iterator_category = std::contiguous_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using element_type      = std::conditional_t<Const, T const, T>;
        using pointer           = std::conditional_t<Const, T const *, T *>;
        using reference         = std::conditional_t<Const, value_type const &, value_type &>;

        constexpr explicit SVIterator() noexcept : _ptr() {}
        constexpr explicit SVIterator(pointer ptr ICHOR_LIFETIME_BOUND) noexcept : _ptr(ptr) {}

        constexpr reference operator*() const noexcept ICHOR_LIFETIME_BOUND { return *_ptr; }
        constexpr pointer operator->() const noexcept ICHOR_LIFETIME_BOUND { return _ptr; }
        constexpr reference operator[](difference_type n) const noexcept ICHOR_LIFETIME_BOUND { return _ptr[n]; }

        constexpr SVIterator& operator++() noexcept { ++_ptr; return *this; }
        constexpr SVIterator operator++(int) noexcept { return SVIterator{++_ptr}; }
        constexpr SVIterator operator+(difference_type n) noexcept { return SVIterator{_ptr + n}; }
        constexpr SVIterator& operator+=(difference_type n) noexcept { _ptr += n; return *this; }

        constexpr SVIterator& operator--() noexcept { --_ptr; return *this; }
        constexpr SVIterator operator--(int) noexcept { return SVIterator{--_ptr}; }
        constexpr SVIterator operator-(difference_type n) noexcept { return SVIterator{_ptr - n}; }
        constexpr SVIterator& operator-=(difference_type n) noexcept { _ptr -= n; return *this; }

        [[nodiscard]] friend constexpr bool operator== (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr == b._ptr; };
        [[nodiscard]] friend constexpr bool operator!= (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr != b._ptr; };
        [[nodiscard]] friend constexpr bool operator<  (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr <  b._ptr; };
        [[nodiscard]] friend constexpr bool operator<= (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr <= b._ptr; };
        [[nodiscard]] friend constexpr bool operator>  (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr >  b._ptr; };
        [[nodiscard]] friend constexpr bool operator>= (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr >= b._ptr; };
        [[nodiscard]] friend constexpr difference_type operator- (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr - b._ptr; };
        [[nodiscard]] friend constexpr SVIterator operator- (SVIterator const & a, difference_type b) noexcept { return a._ptr - b; };
        [[nodiscard]] friend constexpr difference_type operator+ (SVIterator const & a, SVIterator const & b) noexcept { return a._ptr + b._ptr; };
        [[nodiscard]] friend constexpr SVIterator operator+ (SVIterator const & a, difference_type b) noexcept { return a._ptr + b; };
        [[nodiscard]] friend constexpr SVIterator operator+ (difference_type a, SVIterator const & b) noexcept { return a + b._ptr; };

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
        virtual constexpr const_iterator         begin()   const noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr iterator               end()           noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr const_iterator         end()     const noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr reverse_iterator       rbegin()        noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr const_reverse_iterator rbegin()  const noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr reverse_iterator       rend()          noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr const_reverse_iterator rend()    const noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr const_iterator         cbegin()  const noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr const_iterator         cend()    const noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr const_reverse_iterator crbegin() const noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr const_reverse_iterator crend()   const noexcept ICHOR_LIFETIME_BOUND = 0;

        [[nodiscard]] virtual constexpr bool empty() const noexcept = 0;
        [[nodiscard]] virtual constexpr size_type size() const noexcept = 0;
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR virtual constexpr size_type max_size() noexcept = 0;
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR virtual constexpr size_type capacity() noexcept = 0;

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
        virtual constexpr const_reference front() const ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr reference       back() ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr const_reference back() const ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr pointer         data()       noexcept ICHOR_LIFETIME_BOUND = 0;
        virtual constexpr const_pointer   data() const noexcept ICHOR_LIFETIME_BOUND = 0;

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
        [[nodiscard]] friend constexpr bool operator<  (IStaticVector<T> const & a, IStaticVector<T> const & b) noexcept { return a.size() <  b.size() && std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); };
        [[nodiscard]] friend constexpr bool operator<= (IStaticVector<T> const & a, IStaticVector<T> const & b) noexcept { return !(b < a); };
        [[nodiscard]] friend constexpr bool operator>  (IStaticVector<T> const & a, IStaticVector<T> const & b) noexcept { return   b < a; };
        [[nodiscard]] friend constexpr bool operator>= (IStaticVector<T> const & a, IStaticVector<T> const & b) noexcept { return !(a < b); };

    protected:
        ~IStaticVector() = default;
    };

    template <typename T, std::size_t N>
    class StaticVector : public IStaticVector<T> {
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
        constexpr explicit StaticVector(storage_size_type n) noexcept(Detail::is_sufficiently_trivial<T>) {
            assert(n <= N);
            for(storage_size_type i = 0; i < n; ++i) {
                emplace_back();
            }
        }
        constexpr StaticVector(storage_size_type n, const_reference value) {
            assert(n <= N);
            for(storage_size_type i = 0; i < n; ++i) {
                push_back(value);
            }
        }
        template <class InputIterator>
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
                auto pos = list.begin();
                while (pos != list.end()) {
                    if constexpr (std::is_move_constructible_v<T>) {
                        emplace_back(std::move(*pos));
                    } else {
                        push_back(*pos);
                    }
                    ++pos;
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
                emplace_back(std::move(t));
            }
            other.clear();
        }

        constexpr StaticVector& operator=(StaticVector const & other) noexcept(std::is_nothrow_copy_assignable_v<value_type>) {
            assert(other._size <= N);
            clear();
            for(auto const &t : other) {
                push_back(t);
            }
            return *this;
        }
        constexpr StaticVector& operator=(StaticVector&& other) noexcept(std::is_nothrow_move_assignable_v<value_type>) {
            assert(other._size <= N);
            clear();
            for(auto &t : other) {
                emplace_back(std::move(t));
            }
            other.clear();
            return *this;
        }
        template <class InputIterator>
        constexpr void assign(InputIterator first, InputIterator last) {
            size_type i = 0;
            while(first != last) {
                assert(i <= N);
                _size = std::max(_size, static_cast<storage_size_type>(i + 1));
                this->operator[](i) = *first;
                ++i;
                ++first;
            }
        }
        constexpr void assign(size_type n, const_reference u) final {
            assert(n <= N);
            _size = std::max(_size, static_cast<storage_size_type>(n));
            for(size_type i = 0; i < n; ++i) {
                this->operator[](i) = u;
            }
        }

        ~StaticVector() = default;

        ~StaticVector() requires (!Detail::is_sufficiently_trivial<T>) {
            clear();
        }

        constexpr iterator               begin()         noexcept ICHOR_LIFETIME_BOUND final {
            return iterator{_data.data()};
        }
        constexpr const_iterator         begin()   const noexcept ICHOR_LIFETIME_BOUND final {
            return const_iterator{_data.data()};
        }
        constexpr iterator               end()           noexcept ICHOR_LIFETIME_BOUND final {
            return iterator{_data.data() + _size};
        }
        constexpr const_iterator         end()     const noexcept ICHOR_LIFETIME_BOUND final {
            return const_iterator{_data.data() + _size};
        }
        constexpr reverse_iterator       rbegin()        noexcept ICHOR_LIFETIME_BOUND final {
            return reverse_iterator{end()};
        }
        constexpr const_reverse_iterator rbegin()  const noexcept ICHOR_LIFETIME_BOUND final {
            return const_reverse_iterator{end()};
        }
        constexpr reverse_iterator       rend()          noexcept ICHOR_LIFETIME_BOUND final {
            return reverse_iterator{begin()};
        }
        constexpr const_reverse_iterator rend()    const noexcept ICHOR_LIFETIME_BOUND final {
            return const_reverse_iterator{begin()};
        }
        constexpr const_iterator         cbegin()  const noexcept ICHOR_LIFETIME_BOUND final {
            return const_iterator{_data.data()};
        }
        constexpr const_iterator         cend()    const noexcept ICHOR_LIFETIME_BOUND final {
            return const_iterator{_data.data() + _size};
        }
        constexpr const_reverse_iterator crbegin() const noexcept ICHOR_LIFETIME_BOUND final {
            return const_reverse_iterator{cend()};
        }
        constexpr const_reverse_iterator crend()   const noexcept ICHOR_LIFETIME_BOUND final {
            return const_reverse_iterator{cbegin()};
        }

        [[nodiscard]] constexpr bool empty() const noexcept final {
            return _size == 0;
        }
        [[nodiscard]] constexpr size_type size() const noexcept final {
            return _size;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr size_type max_size() noexcept final {
            return N;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr size_type capacity() noexcept final {
            return N;
        }

        constexpr void resize(size_type sz, const_reference c) final {
            assert(sz <= std::numeric_limits<storage_size_type>::max());
            assert(sz <= N);
            if constexpr (Detail::is_sufficiently_trivial<T>) {
                if(sz > _size) {
                    for(storage_size_type i = _size; i < sz; ++i) {
                        if constexpr (std::is_move_constructible_v<T>) {
                            emplace_back(c);
                        } else {
                            push_back(c);
                        }
                    }
                }
            } else {
                if(sz > _size) {
                    for(storage_size_type i = _size; i < sz; ++i) {
                        if constexpr (std::is_move_constructible_v<T>) {
                            emplace_back(c);
                        } else {
                            push_back(c);
                        }
                    }
                } else if(sz < _size) {
                    for(storage_size_type i = static_cast<storage_size_type>(sz); i < _size; ++i) {
                        std::destroy_at(_data.data() + i);
                    }
                }
            }
            _size = static_cast<storage_size_type>(sz);
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
            return *_data.data();
        }
        constexpr const_reference front() const ICHOR_LIFETIME_BOUND final {
            return *_data.data();
        }
        constexpr reference       back() ICHOR_LIFETIME_BOUND final {
            return *(_data.data() + (_size - 1));
        }
        constexpr const_reference back() const ICHOR_LIFETIME_BOUND final {
            return *(_data.data() + (_size - 1));
        }
        constexpr pointer         data()       noexcept ICHOR_LIFETIME_BOUND final {
            return std::addressof(front());
        }
        constexpr const_pointer   data() const noexcept ICHOR_LIFETIME_BOUND final {
            return std::addressof(front());
        }

        constexpr iterator insert(const_iterator const_position, const_reference x) ICHOR_LIFETIME_BOUND final {
            assert(_size < N);
            auto position = iterator{const_cast<pointer>(const_position.operator->())};
            ++_size;
            if constexpr (std::is_move_constructible_v<T>) {
                std::move_backward(position, end() - 1, end());
            } else {
                std::copy_backward(position, end() - 1, end());
            }

            std::construct_at(position.operator->(), x);

            return position;
        }

        constexpr iterator insert(const_iterator const_position, value_type&& x) ICHOR_LIFETIME_BOUND final {
            assert(_size < N);
            auto position = iterator{const_cast<pointer>(const_position.operator->())};
            ++_size;
            if constexpr (std::is_move_constructible_v<T>) {
                std::move_backward(position, end() - 1, end());
            } else {
                std::terminate(); // should never be called because value_type&& is impossible for non-moveable types
            }

            if constexpr (std::is_move_constructible_v<T>) {
                std::construct_at(position.operator->(), std::move(x));
            }

            return position;
        }

        constexpr iterator insert(const_iterator const_position, size_type n, const_reference x) ICHOR_LIFETIME_BOUND final {
            assert(_size + n <= N);
            auto position = iterator{const_cast<pointer>(const_position.operator->())};
            _size += static_cast<storage_size_type>(n);
            if constexpr (std::is_move_constructible_v<T>) {
                std::move_backward(position, end() - static_cast<difference_type>(n), end());
            } else {
                std::copy_backward(position, end() - static_cast<difference_type>(n), end());
            }

            for(difference_type i = 0; i < static_cast<difference_type>(n); ++i) {
                std::construct_at((position + i).operator->(), x);
            }

            return position;
        }
        template <class InputIterator>
        constexpr iterator insert(const_iterator const_position, InputIterator first, InputIterator last) ICHOR_LIFETIME_BOUND {
            difference_type n = std::distance(first, last);
            assert(n >= 0);
            assert(_size + (std::size_t)n <= N);
            auto position = iterator{const_cast<pointer>(const_position.operator->())};
            _size += static_cast<decltype(_size)>(n);
            if constexpr (std::is_move_constructible_v<T>) {
                std::move_backward(position, end() - n, end());
            } else {
                std::copy_backward(position, end() - n, end());
            }

            difference_type i = 0;
            while(first != last) {
                std::construct_at((position + i).operator->(), *first);
                ++i;
                ++first;
            }

            return position;
        }

        template <class... Args>
        constexpr iterator emplace(const_iterator const_position, Args&&... args) ICHOR_LIFETIME_BOUND {
            assert(_size < N);
            auto position = iterator{const_cast<pointer>(const_position.operator->())};
            ++_size;
            if constexpr (std::is_move_constructible_v<T>) {
                std::move_backward(position, end() - 1, end());
            } else {
                std::copy_backward(position, end() - 1, end());
            }

            std::construct_at(position.operator->(), std::forward<Args>(args)...);

            return position;
        }
        template <class... Args>
        constexpr reference emplace_back(Args&&... args) noexcept(Detail::is_sufficiently_trivial<T>) ICHOR_LIFETIME_BOUND {
            assert(_size < N);
            //fmt::print("emplace_back {} {}\n", _size, (void*)(data() + size()));
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
                std::construct_at(data() + size(), std::forward<T>(x));
            } else {
                std::terminate(); // should never be called because value_type&& is impossible for non-moveable types
            }
            ++_size;
        }

        constexpr void pop_back() noexcept(Detail::is_sufficiently_trivial<T>) final {
            assert(_size > 0);
            if constexpr (!Detail::is_sufficiently_trivial<T>) {
                //fmt::print("pop_back destroy_at {} {}\n", (void*)begin().operator->(), (void*)&back());
                std::destroy_at(std::addressof(back()));
            }
            --_size;
        }
        constexpr iterator erase(const_iterator const_position) ICHOR_LIFETIME_BOUND final {
            auto position = iterator{const_cast<pointer>(const_position.operator->())};
            if(position + 1 != end()) {
                if constexpr (std::is_move_constructible_v<T>) {
                    std::move(position + 1, end(), position);
                } else {
                    std::copy(position + 1, end(), position);
                }
            }
            //fmt::print("erase pop_back {} {}\n", (void*)begin().operator->(), (void*)position.operator->());
            pop_back();
            return position;
        }
        constexpr iterator erase(const_iterator const_first, const_iterator const_last) ICHOR_LIFETIME_BOUND final {
            auto first = iterator{const_cast<pointer>(const_first.operator->())};
            if(const_first != const_last) {
                auto last = iterator{const_cast<pointer>(const_last.operator->())};
                SVIterator<T, false> position;

                if constexpr (std::is_move_constructible_v<T>) {
                    position = std::move(last, end(), first);
                } else {
                    position = std::copy(last, end(), first);
                }

                if constexpr (!Detail::is_sufficiently_trivial<T>) {
                    while(position != end()) {
                        std::destroy_at(position.operator->());
                        ++position;
                    }
                }
                _size -= static_cast<storage_size_type>(std::distance(first, last));
            }

            return first;
        }

        constexpr void clear() noexcept final {
            if constexpr (!Detail::is_sufficiently_trivial<T>) {
                for(storage_size_type i = 0; i < _size; ++i) {
                    std::destroy_at(_data.data() + i);
                }
            }
            _size = 0;
        }

        constexpr void swap(StaticVector &x) noexcept(std::is_nothrow_swappable_v<T> && std::is_nothrow_move_constructible_v<T>) {
            assert(x.size() <= N);
            assert(size() <= x.max_size());
            static_assert(std::is_swappable_v<T>, "T is not swappable, implement swap on the type");
            auto greater = static_cast<difference_type>(std::max(size(), x.size()));

            for(difference_type i = 0; i < greater; ++i) {
                using std::swap;
                swap((begin() + i).operator*(), (x.begin() + i).operator*());
            }

            std::swap(_size, x._size);
        }

    private:
        Detail::UninitializedArray<T, N> _data;
        storage_size_type _size{};
    };
}

namespace std {
    template <typename T, size_t N>
    constexpr bool operator==(const Ichor::v1::StaticVector<T, N>& a, const Ichor::v1::StaticVector<T, N>& b) {
        return a == b;
    }
    template <typename T, size_t N>
    constexpr bool operator!=(const Ichor::v1::StaticVector<T, N>& a, const Ichor::v1::StaticVector<T, N>& b) {
        return a != b;
    }
    template <typename T, size_t N>
    constexpr bool operator<(const Ichor::v1::StaticVector<T, N>& a, const Ichor::v1::StaticVector<T, N>& b) {
        return a < b;
    }
    template <typename T, size_t N>
    constexpr bool operator<=(const Ichor::v1::StaticVector<T, N>& a, const Ichor::v1::StaticVector<T, N>& b) {
        return a <= b;
    }
    template <typename T, size_t N>
    constexpr bool operator>(const Ichor::v1::StaticVector<T, N>& a, const Ichor::v1::StaticVector<T, N>& b) {
        return a > b;
    }
    template <typename T, size_t N>
    constexpr bool operator>=(const Ichor::v1::StaticVector<T, N>& a, const Ichor::v1::StaticVector<T, N>& b) {
        return a >= b;
    }
    template <typename T>
    constexpr bool operator==(const Ichor::v1::IStaticVector<T>& a, const Ichor::v1::IStaticVector<T>& b) {
        return a == b;
    }
    template <typename T>
    constexpr bool operator!=(const Ichor::v1::IStaticVector<T>& a, const Ichor::v1::IStaticVector<T>& b) {
        return a != b;
    }
    template <typename T>
    constexpr bool operator<(const Ichor::v1::IStaticVector<T>& a, const Ichor::v1::IStaticVector<T>& b) {
        return a < b;
    }
    template <typename T>
    constexpr bool operator<=(const Ichor::v1::IStaticVector<T>& a, const Ichor::v1::IStaticVector<T>& b) {
        return a <= b;
    }
    template <typename T>
    constexpr bool operator>(const Ichor::v1::IStaticVector<T>& a, const Ichor::v1::IStaticVector<T>& b) {
        return a > b;
    }
    template <typename T>
    constexpr bool operator>=(const Ichor::v1::IStaticVector<T>& a, const Ichor::v1::IStaticVector<T>& b) {
        return a >= b;
    }

    template<typename T, size_t N>
    constexpr void swap(Ichor::v1::StaticVector<T, N> &x, Ichor::v1::StaticVector<T, N> &y)
    noexcept(noexcept(x.swap(y))) {
        x.swap(y);
    }
}
