#pragma once

extern std::atomic<uint64_t> idCounter;

template <int SIZE>
struct buffer_resource final : public std::pmr::memory_resource {
    buffer_resource() : id(idCounter++) {
        top_block = reinterpret_cast<block*>(buffer.data());
    }

    ~buffer_resource() final = default;

private:
    struct block {
        std::size_t size;
        block *next;
        bool used;
    };

    std::size_t round_up(std::size_t bytes, std::size_t alignment)
    {
        if (alignment == 0) {
            return bytes;
        }

        auto remainder = bytes % alignment;
        if (remainder == 0) {
            return bytes;
        }

        return bytes + alignment - remainder;
    }

    [[nodiscard]]
    void* do_allocate(std::size_t bytes, std::size_t alignment) final {
        if (bytes == 0) {
            bytes = 1; // Ensures we don't return the same pointer twice.
        }
        auto bytes_necessary = round_up(bytes + sizeof(block), alignment);

        auto *last_used_block = top_block;
        std::size_t total_used = 0;
        while(last_used_block->used || (last_used_block->next != nullptr && last_used_block->size < bytes_necessary)) {
            total_used += last_used_block->size;
            last_used_block = last_used_block->next;
        }

        if(total_used + bytes_necessary > SIZE) {
#ifndef NDEBUG //warning: fmt::print calls printf(), which allocates (at least on my machine)
            fmt::print("Ran out of space on allocator {:L}\n", id);
#endif
            std::terminate();
        }

        auto unaligned_location = reinterpret_cast<std::size_t>(buffer.data()) + total_used + sizeof(block);
        auto location = round_up(unaligned_location, alignment);

#ifndef NDEBUG //warning: fmt::print calls printf(), which allocates (at least on my machine)
        if(location - unaligned_location > bytes_necessary - bytes) {
            fmt::print("Alignment error, check implementation\n");
            std::terminate();
        }
#endif

        if(last_used_block->next == nullptr) {
#ifndef NDEBUG //warning: fmt::print calls printf(), which allocates (at least on my machine)
            fmt::print("id {:L} allocated {:L} bytes for {:L} bytes requested with alignment {:L} at location {:L} and returning {:L} and block {:L}\n", id, bytes_necessary,
                       bytes, alignment, unaligned_location, location, reinterpret_cast<std::size_t>(last_used_block));
#endif
            last_used_block->size = bytes_necessary;
            last_used_block->next = reinterpret_cast<block*>(buffer.data() + total_used + bytes_necessary);
        }
#ifndef NDEBUG //warning: fmt::print calls printf(), which allocates (at least on my machine)
        else {
            fmt::print(
                    "id {:L} allocated {:L} bytes for {:L} bytes requested with alignment {:L} at location {:L} and returning {:L} by re-using block of size {}\n",
                    id, bytes_necessary,
                    bytes, alignment, unaligned_location, location, last_used_block->size);
        }
#endif
        last_used_block->used = true;

        return reinterpret_cast<block*>(location);
    }

    void do_deallocate(void *p, size_t bytes, size_t alignment) final {
        auto *last_used_block = top_block;

        while(last_used_block != nullptr) {
            if(reinterpret_cast<std::size_t>(last_used_block) == reinterpret_cast<std::size_t>(p) - sizeof(block)) {
                last_used_block->used = false;

#ifndef NDEBUG //warning: fmt::print calls printf(), which allocates (at least on my machine)
                fmt::print("id {:L} deallocated {:L} bytes at location {:L} and block {:L}\n", id, bytes, reinterpret_cast<std::size_t>(p), reinterpret_cast<std::size_t>(last_used_block));
#endif

                break;
            }

            last_used_block = last_used_block->next;
        }
    }

    [[nodiscard]]
    bool do_is_equal(const memory_resource& other) const noexcept final {
        return this == &other;
    }

    alignas(alignof(std::max_align_t)) std::array<char, SIZE> buffer{};
    block *top_block{};
    uint64_t id;
};

struct terminating_resource final : public std::pmr::memory_resource {
    [[nodiscard]]
    void* do_allocate(size_t bytes, size_t alignment) final {
        std::terminate();
    }

    void do_deallocate(void *p, size_t bytes, size_t alignment) final {
        std::terminate();
    }

    [[nodiscard]]
    bool do_is_equal(const memory_resource& other) const noexcept final {
        return this == &other;
    }
};