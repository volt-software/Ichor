#include "Common.h"
#include <ichor/CommunicationChannel.h>
#include <ichor/event_queues/BaseEventQueue.h>
#include <array>
#include <latch>
#include <string>
#include <thread>
#include <vector>

namespace {
    struct BroadcastStringEvent final : public Event {
        BroadcastStringEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority, std::string _str) noexcept :
                Event(_id, _originatingService, _priority), str(std::move(_str)) {}
        ~BroadcastStringEvent() final = default;

        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr std::string_view get_name() const noexcept final {
            return NAME;
        }
        [[nodiscard]] ICHOR_CONST_FUNC_ATTR constexpr NameHashType get_type() const noexcept final {
            return TYPE;
        }

        std::string str;
        static constexpr NameHashType TYPE = typeNameHash<BroadcastStringEvent>();
        static constexpr std::string_view NAME = typeName<BroadcastStringEvent>();
    };

    /// Never runs, only keeps the events pushed into it.
    struct RecordingQueue final : public BaseEventQueue {
        [[nodiscard]] bool empty() const final {
            return events.empty();
        }
        [[nodiscard]] uint64_t size() const final {
            return events.size();
        }
        [[nodiscard]] bool is_running() const noexcept final {
            return false;
        }
        [[nodiscard]] NameHashType get_queue_name_hash() const noexcept final {
            return typeNameHash<RecordingQueue>();
        }
        bool start(bool) final {
            return false;
        }
        [[nodiscard]] bool shouldQuit() final {
            return false;
        }
        void quit() final {
        }
        void pushEventInternal(uint64_t, std::unique_ptr<Event> &&event) final {
            events.push_back(std::move(event));
        }

        std::vector<std::unique_ptr<Event>> events;
    };
}

TEST_CASE("CommunicationChannelTests") {
    SECTION("broadcastEvent gives every other manager the arguments") {
        CommunicationChannel channel{};
        // A thread can have only one manager, which also has to be destroyed on that thread
        std::array<RecordingQueue *, 3> queues{};
        std::latch created{queues.size()};
        std::latch broadcasted{1};
        std::vector<std::thread> threads;
        threads.reserve(queues.size());
        for(RecordingQueue *&queuePtr : queues) {
            threads.emplace_back([&channel, &created, &broadcasted, &queuePtr]() {
                RecordingQueue queue;
                channel.addManager(&queue.getManager());
                queuePtr = &queue;
                created.count_down();
                broadcasted.wait();
            });
        }
        created.wait();

        // Longer than any small string buffer, so that a moved-from copy is empty
        std::string const str(100, 'b');
        channel.broadcastEvent<BroadcastStringEvent>(queues[0]->getManager(), ServiceIdType{1}, std::string{str});

        std::vector<std::vector<std::string>> received;
        for(RecordingQueue *queue : queues) {
            auto &strs = received.emplace_back();
            for(auto const &evt : queue->events) {
                if(evt->get_type() == BroadcastStringEvent::TYPE) {
                    strs.push_back(static_cast<BroadcastStringEvent const &>(*evt).str);
                }
            }
        }
        broadcasted.count_down();
        for(auto &t : threads) {
            t.join();
        }

        REQUIRE(received[0].empty());
        REQUIRE(received[1] == std::vector<std::string>{str});
        REQUIRE(received[2] == std::vector<std::string>{str});
    }
}
