#pragma once

#include <ichor/events/Event.h>
#include <vector>

namespace Ichor {
    struct NetworkDataEvent final : public Event {
        explicit NetworkDataEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority, std::vector<uint8_t>&& data) noexcept :
                Event(_id, _originatingService, _priority), _data(std::move(data)), _movedFrom(false) {}
        ~NetworkDataEvent() final = default;

        [[nodiscard]] std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] NameHashType get_type() const noexcept final {
            return TYPE;
        }

        static constexpr NameHashType TYPE = typeNameHash<NetworkDataEvent>();
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
        Event(_id, _originatingService, _priority), data(std::move(_data)), msgId(_msgId) {}
        ~FailedSendMessageEvent() final = default;

        [[nodiscard]] std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] NameHashType get_type() const noexcept final {
            return TYPE;
        }

        static constexpr NameHashType TYPE = typeNameHash<FailedSendMessageEvent>();
        static constexpr std::string_view NAME = typeName<FailedSendMessageEvent>();

        mutable std::vector<uint8_t> data;
        uint64_t msgId;
    };
}
