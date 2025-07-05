#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/events/Event.h>
#include <ichor/stl/ErrnoUtils.h>
#include <ichor/stl/StringUtils.h>
#include <ichor/dependency_management/IService.h>
#include <ichor/event_queues/IEventQueue.h>
#include <tl/expected.h>
#include <functional>

struct io_uring;
struct io_uring_sqe;
struct io_uring_cqe;
struct io_uring_buf_ring;

namespace Ichor {
    using ProvidedBufferIdType = int;
    struct IOUringBuf final {
        IOUringBuf() = default;
        IOUringBuf(io_uring *eventQueue, io_uring_buf_ring *bufRing, char *entriesBuf, unsigned short entries, unsigned int entryBufferSize, ProvidedBufferIdType id);
        IOUringBuf(IOUringBuf const &) = delete;
        IOUringBuf(IOUringBuf &&) noexcept;
        IOUringBuf& operator=(IOUringBuf const &) = delete;
        IOUringBuf& operator=(IOUringBuf&&) noexcept;
        ~IOUringBuf();
        [[nodiscard]] std::string_view readMemory(unsigned int entry) const noexcept;
        void markEntryAvailableAgain(unsigned short entry) noexcept;
        [[nodiscard]] unsigned int getEntries() const noexcept {
            return _entries;
        }
        [[nodiscard]] ProvidedBufferIdType getBufferGroupId() const noexcept {
            return _bgid;
        }
    private:
        io_uring *_eventQueue{};
        io_uring_buf_ring* _bufRing;
        char *_entriesBuf;
        unsigned short _entries;
        unsigned int _entryBufferSize;
        ProvidedBufferIdType _bgid;
        int _mask;
    };

    static_assert(std::is_default_constructible_v<IOUringBuf>, "IOUringBuf is required to be default constructible");
    static_assert(std::is_destructible_v<IOUringBuf>, "IOUringBuf is required to be destructible");
    static_assert(!std::is_copy_constructible_v<IOUringBuf>, "IOUringBuf is required to not be copy constructible");
    static_assert(!std::is_copy_assignable_v<IOUringBuf>, "IOUringBuf is required to not be copy assignable");
    static_assert(std::is_move_constructible_v<IOUringBuf>, "IOUringBuf is required to be move constructible");
    static_assert(std::is_move_assignable_v<IOUringBuf>, "IOUringBuf is required to be move assignable");

    class IIOUringQueue : public IEventQueue {
    public:
        ~IIOUringQueue() override = default;
        [[nodiscard]] virtual v1::NeverNull<io_uring*> getRing() noexcept = 0;
        [[nodiscard]] virtual unsigned int getMaxEntriesCount() const noexcept = 0;
        [[nodiscard]] virtual uint32_t sqeSpaceLeft() const noexcept = 0;
        virtual void submitIfNeeded() = 0;
        virtual void forceSubmit() = 0;
        virtual void submitAndWait(uint32_t waitNr) = 0;
        [[nodiscard]] virtual io_uring_sqe* getSqe() noexcept = 0;
        virtual io_uring_sqe* getSqeWithData(IService *self, std::function<void(io_uring_cqe*)> fun) noexcept = 0;
        virtual io_uring_sqe* getSqeWithData(ServiceIdType serviceId, std::function<void(io_uring_cqe*)> fun) noexcept = 0;
        [[nodiscard]] virtual v1::Version getKernelVersion() const noexcept = 0;
        ///
        /// \param entries no. of entries in the to-be-created buffer. Cannot be 0, cannot be larger than 32768 and has to be a power of two
        /// \return
        [[nodiscard]] virtual tl::expected<IOUringBuf, v1::IOError> createProvidedBuffer(unsigned short entries, unsigned int entryBufferSize) noexcept = 0;
    };
}
