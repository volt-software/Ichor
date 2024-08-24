#pragma once

#include <vector>
#include <algorithm>

// The SectionalPriorityQueue is specialized for one purpose only: an ordered, vector-mapped queue for unique_ptr's, which only gets pushed/popped to.
// This ensures that the compare function never gets un-set unique_ptrs. (except when they get pushed, which this queue considers a user-mistake, for performance reasons)

namespace Ichor {
    template <typename T, typename Compare>
    struct SectionalPriorityQueue final {
        [[nodiscard]] bool empty() const noexcept {
            return c.empty();
        }

        [[nodiscard]] typename std::vector<T>::size_type size() const noexcept {
            return c.size();
        }

        [[nodiscard]] typename std::vector<T>::size_type maxSize() const noexcept {
            return c.max_size();
        }

        void push(T const &t) {
            c.push_back(t);
            std::push_heap(c.begin(), c.end(), Compare{});
        }

        void push(T &&t) {
            c.push_back(std::forward<T>(t));
            std::push_heap(c.begin(), c.end(), Compare{});
        }

        template <typename... Args>
        void emplace(Args&&... args) {
            c.emplace_back(std::forward<Args>(args)...);
            std::push_heap(c.begin(), c.end(), Compare{});
        }

        /**
         * pops the last element in the vector and returns it (note: different than std::priority_queue!)
         * @return
         */
        T pop() {
            std::pop_heap(c.begin(), c.end(), Compare{});
            T t = std::move(c.back());
            c.pop_back();
            return t;
        }

        void reserve(typename std::vector<T>::size_type size) {
            c.reserve(size);
        }

    private:
        std::vector<T> c;
    };
}
