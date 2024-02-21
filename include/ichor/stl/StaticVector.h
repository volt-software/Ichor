#pragma once

#include <cstddef>
#include <type_traits>
#include <iterator>
#include <array>
#include <limits>
#include <memory>
#include <algorithm>

// An implementation that is close to the StaticVector proposal: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p0843r6.html
// Difference being that this implementation also supports type-erasure
// Shoutout to David Stone, more info at https://www.youtube.com/watch?v=I8QJLGI0GOE
namespace Ichor {

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
            constexpr auto data() noexcept -> T * {
                if constexpr (is_sufficiently_trivial<T>) {
                    return static_cast<T *>(_storage.data());
                } else {
                    return reinterpret_cast<T *>(_storage.buff);
                }
            }
            constexpr auto data() const noexcept -> T const * {
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
            constexpr auto data() noexcept -> T * {
                return nullptr;
            }
            constexpr auto data() const noexcept -> T const * {
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
        constexpr explicit SVIterator(pointer ptr) noexcept : _ptr(ptr) {}

        constexpr reference operator*() const noexcept { return *_ptr; }
        constexpr pointer operator->() const noexcept { return _ptr; }
        constexpr reference operator[](difference_type n) const noexcept { return _ptr[n]; }

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

        virtual constexpr iterator               begin()         noexcept = 0;
        virtual constexpr const_iterator         begin()   const noexcept = 0;
        virtual constexpr iterator               end()           noexcept = 0;
        virtual constexpr const_iterator         end()     const noexcept = 0;
        virtual constexpr reverse_iterator       rbegin()        noexcept = 0;
        virtual constexpr const_reverse_iterator rbegin()  const noexcept = 0;
        virtual constexpr reverse_iterator       rend()          noexcept = 0;
        virtual constexpr const_reverse_iterator rend()    const noexcept = 0;
        virtual constexpr const_iterator         cbegin()  const noexcept = 0;
        virtual constexpr const_iterator         cend()    const noexcept = 0;
        virtual constexpr const_reverse_iterator crbegin() const noexcept = 0;
        virtual constexpr const_reverse_iterator crend()   const noexcept = 0;

        [[nodiscard]] virtual constexpr bool empty() const noexcept = 0;
        [[nodiscard]] virtual constexpr size_type size() const noexcept = 0;
        [[nodiscard]] virtual constexpr size_type max_size() noexcept = 0;
        [[nodiscard]] virtual constexpr size_type capacity() noexcept = 0;

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

        virtual constexpr reference       operator[](size_type n) = 0;
        virtual constexpr const_reference operator[](size_type n) const = 0;
        virtual constexpr reference       front() = 0;
        virtual constexpr const_reference front() const = 0;
        virtual constexpr reference       back() = 0;
        virtual constexpr const_reference back() const = 0;
        virtual constexpr pointer         data()       noexcept = 0;
        virtual constexpr const_pointer   data() const noexcept = 0;

        virtual constexpr void push_back(const_reference x) = 0;
        virtual constexpr void push_back(value_type&& x) = 0;

        virtual constexpr void pop_back() = 0;
        virtual constexpr iterator erase(const_iterator position) = 0;
        virtual constexpr iterator erase(const_iterator first, const_iterator last) = 0;

        virtual constexpr void clear() noexcept = 0;

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
        constexpr StaticVector(storage_size_type n, value_type const & value) {
            assert(n <= N);
            for(storage_size_type i = 0; i < n; ++i) {
                emplace_back(value);
            }
        }
        template <class InputIterator>
        constexpr StaticVector(InputIterator first, InputIterator last) {
            while(first != last) {
                emplace_back(*first);
                first++;
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
                    emplace_back(*pos);
                    pos++;
                }
            }
        }
        constexpr StaticVector(StaticVector const & other) noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
            assert(other._size <= N);
            for(auto const &t : other) {
                emplace_back(t);
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
                emplace_back(t);
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

        constexpr iterator               begin()         noexcept final {
            return iterator{_data.data()};
        }
        constexpr const_iterator         begin()   const noexcept final {
            return const_iterator{_data.data()};
        }
        constexpr iterator               end()           noexcept final {
            return iterator{_data.data() + _size};
        }
        constexpr const_iterator         end()     const noexcept final {
            return const_iterator{_data.data() + _size};
        }
        constexpr reverse_iterator       rbegin()        noexcept final {
            return reverse_iterator{end()};
        }
        constexpr const_reverse_iterator rbegin()  const noexcept final {
            return const_reverse_iterator{end()};
        }
        constexpr reverse_iterator       rend()          noexcept final {
            return reverse_iterator{begin()};
        }
        constexpr const_reverse_iterator rend()    const noexcept final {
            return const_reverse_iterator{begin()};
        }
        constexpr const_iterator         cbegin()  const noexcept final {
            return const_iterator{_data.data()};
        }
        constexpr const_iterator         cend()    const noexcept final {
            return const_iterator{_data.data() + _size};
        }
        constexpr const_reverse_iterator crbegin() const noexcept final {
            return const_reverse_iterator{cend()};
        }
        constexpr const_reverse_iterator crend()   const noexcept final {
            return const_reverse_iterator{cbegin()};
        }

        [[nodiscard]] constexpr bool empty() const noexcept final {
            return _size == 0;
        }
        [[nodiscard]] constexpr size_type size() const noexcept final {
            return _size;
        }
        [[nodiscard]] constexpr size_type max_size() noexcept final {
            return N;
        }
        [[nodiscard]] constexpr size_type capacity() noexcept final {
            return N;
        }

        constexpr void resize(size_type sz, const_reference c) final {
            assert(sz <= std::numeric_limits<storage_size_type>::max());
            assert(sz <= N);
            if constexpr (Detail::is_sufficiently_trivial<T>) {
                if(sz > _size) {
                    for(storage_size_type i = _size; i < sz; ++i) {
                        emplace_back(c);
                    }
                }
            } else {
                if(sz > _size) {
                    for(storage_size_type i = _size; i < sz; ++i) {
                        emplace_back(c);
                    }
                } else if(sz < _size) {
                    for(storage_size_type i = static_cast<storage_size_type>(sz); i < _size; ++i) {
                        std::destroy_at(_data.data() + i);
                    }
                }
            }
            _size = static_cast<storage_size_type>(sz);
        }

        constexpr reference       operator[](size_type n) final {
            assert(n < _size);
            return *(_data.data() + n);
        }
        constexpr const_reference operator[](size_type n) const final {
            assert(n < _size);
            return *(_data.data() + n);
        }
        constexpr reference       front() final {
            return *_data.data();
        }
        constexpr const_reference front() const final {
            return *_data.data();
        }
        constexpr reference       back() final {
            return *(_data.data() + (_size - 1));
        }
        constexpr const_reference back() const final {
            return *(_data.data() + (_size - 1));
        }
        constexpr pointer         data()       noexcept final {
            return std::addressof(front());
        }
        constexpr const_pointer   data() const noexcept final {
            return std::addressof(front());
        }

        constexpr iterator insert(const_iterator position, const_reference x);
        constexpr iterator insert(const_iterator position, value_type&& x);
        constexpr iterator insert(const_iterator position, storage_size_type n, const_reference x);
        template <class InputIterator>
        constexpr iterator insert(const_iterator position, InputIterator first, InputIterator last);

        template <class... Args>
        constexpr iterator emplace(const_iterator position, Args&&... args);
        template <class... Args>
        constexpr reference emplace_back(Args&&... args) {
            assert(_size < N);
            std::construct_at(data() + size(), T{std::forward<Args>(args)...});
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
            std::construct_at(data() + size(), std::forward<T>(x));
            ++_size;
        }

        constexpr void pop_back() final {
            assert(_size > 0);
            if constexpr (!Detail::is_sufficiently_trivial<T>) {
                std::destroy_at(std::addressof(back()));
            }
            --_size;
        }
        constexpr iterator erase(const_iterator const_position) final {
            auto position = iterator{const_cast<pointer>(const_position.operator->())};
            if(position + 1 != end()) {
                std::move(position + 1, end(), position);
            }
            pop_back();
            return position;
        }
        constexpr iterator erase(const_iterator const_first, const_iterator const_last) final {
            auto first = iterator{const_cast<pointer>(const_first.operator->())};
            if(const_first != const_last) {
                auto last = iterator{const_cast<pointer>(const_last.operator->())};
                auto position = std::move(last, end(), first);
                if constexpr (!Detail::is_sufficiently_trivial<T>) {
                    while(position != end()) {
                        std::destroy_at(position.operator->());
                        position++;
                    }
                }
                _size -= std::distance(first, last);
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

    private:
        Detail::UninitializedArray<T, N> _data;
        storage_size_type _size{};
    };
}
