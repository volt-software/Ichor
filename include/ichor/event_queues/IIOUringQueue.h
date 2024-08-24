#pragma once

#ifndef ICHOR_USE_LIBURING
#error "Ichor has not been compiled with io_uring support"
#endif

#include <ichor/stl/NeverAlwaysNull.h>
#include <ichor/events/Event.h>
#include <ichor/ConstevalHash.h>
#include <ichor/stl/ErrnoUtils.h>
#include <ichor/stl/StringUtils.h>
#include <functional>

struct io_uring;
struct io_uring_sqe;

namespace Ichor {
    struct UringResponseEvent final : public Event {
        explicit UringResponseEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::function<void(int32_t)> _fun) noexcept :
                Event(_id, _originatingService, _priority), fun(std::move(_fun)) {}
        ~UringResponseEvent() final = default;

        [[nodiscard]] std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] NameHashType get_type() const noexcept final {
            return TYPE;
        }

        std::function<void(int32_t)> fun;
        int32_t res{};
        static constexpr NameHashType TYPE = typeNameHash<UringResponseEvent>();
        static constexpr std::string_view NAME = typeName<UringResponseEvent>();
    };

    class IIOUringQueue {
    public:
        virtual ~IIOUringQueue() = default;
        [[nodiscard]] virtual NeverNull<io_uring*> getRing() noexcept = 0;
        virtual unsigned int getMaxEntriesCount() const noexcept = 0;
        [[nodiscard]] virtual uint64_t getNextEventIdFromIchor() noexcept = 0;
        virtual uint32_t sqeSpaceLeft() const noexcept = 0;
        virtual void submitIfNeeded() = 0;
        virtual void submitAndWait(uint32_t waitNr) = 0;
        [[nodiscard]] virtual io_uring_sqe *getSqe() noexcept = 0;
        [[nodiscard]] virtual Version getKernelVersion() const noexcept = 0;
    };
}
