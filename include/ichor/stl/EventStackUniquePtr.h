#pragma once


namespace Ichor {
    class [[nodiscard]] EventStackUniquePtr final {
    public:
        EventStackUniquePtr() = default;

        template<typename T, typename... Args>
        requires Derived<T, Event>
        static EventStackUniquePtr create(Args &&... args) {
            static_assert(sizeof(T) <= 128, "size not big enough to hold T");
            static_assert(T::TYPE != 0, "type of T cannot be 0");
            EventStackUniquePtr ptr;
            new(ptr._buffer.data()) T(std::forward<Args>(args)...);
            ptr._type = T::TYPE;
            return ptr;
        }

        EventStackUniquePtr(const EventStackUniquePtr &) = delete;

        EventStackUniquePtr(EventStackUniquePtr &&other) noexcept {
            if (!_empty) {
                reinterpret_cast<Event *>(_buffer.data())->~Event();
            }
            _buffer = other._buffer;
            other._empty = true;
            _empty = false;
            _type = other._type;
        }


        EventStackUniquePtr &operator=(const EventStackUniquePtr &) = delete;

        EventStackUniquePtr &operator=(EventStackUniquePtr &&other) noexcept {
            if (!_empty) {
                reinterpret_cast<Event *>(_buffer.data())->~Event();
            }
            _buffer = other._buffer;
            other._empty = true;
            _empty = false;
            _type = other._type;
            return *this;
        }

        ~EventStackUniquePtr() {
            if (!_empty) {
                reinterpret_cast<Event *>(_buffer.data())->~Event();
            }
        }

        template<typename T>
        requires Derived<T, Event>
        [[nodiscard]] T *getT() {
            if (_empty) {
                throw std::runtime_error("empty");
            }

            return reinterpret_cast<T *>(_buffer.data());
        }

        [[nodiscard]] Event *get() {
            if (_empty) {
                throw std::runtime_error("empty");
            }

            return reinterpret_cast<Event *>(_buffer.data());
        }

        [[nodiscard]] uint64_t getType() const noexcept {
            return _type;
        }

    private:

        std::array<uint8_t, 128> _buffer;
        uint64_t _type{0};
        bool _empty{true};
    };
}