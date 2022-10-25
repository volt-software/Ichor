#pragma once

#include <ichor/events/Event.h>

namespace Ichor {
    struct NetworkDataEvent final : public Event {
        explicit NetworkDataEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::vector<uint8_t>&& data) noexcept :
                Event(TYPE, NAME, _id, _originatingService, _priority), _data(std::forward<std::vector<uint8_t>>(data)), _movedFrom(false) {}
        ~NetworkDataEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<NetworkDataEvent>();
        static constexpr std::string_view NAME = typeName<NetworkDataEvent>();

        std::vector<uint8_t>& getData() const {
            if(_movedFrom) {
                throw std::runtime_error("already moved from");
            }

            return _data;
        }

        std::vector<uint8_t> moveData() {
            if(_movedFrom) {
                throw std::runtime_error("already moved from");
            }

            _movedFrom = true;
            return std::move(_data);
        }
    private:
        mutable std::vector<uint8_t> _data;
        mutable bool _movedFrom;
    };

    struct FailedSendMessageEvent final : public Event {
        explicit FailedSendMessageEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::vector<uint8_t>&& _data, uint64_t _msgId) noexcept :
        Event(TYPE, NAME, _id, _originatingService, _priority), data(std::forward<std::vector<uint8_t>>(_data)), msgId(_msgId) {}
        ~FailedSendMessageEvent() final = default;

        static constexpr uint64_t TYPE = typeNameHash<FailedSendMessageEvent>();
        static constexpr std::string_view NAME = typeName<FailedSendMessageEvent>();

        mutable std::vector<uint8_t> data;
        uint64_t msgId;
    };
}