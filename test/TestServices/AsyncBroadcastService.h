#pragma once

using namespace Ichor;
using namespace Ichor::v1;

extern std::unique_ptr<Ichor::AsyncManualResetEvent> _evt;
extern std::atomic<uint64_t> evtGate;

struct CustomDoWorkEvent final : public Event {
    CustomDoWorkEvent(uint64_t _id, ServiceIdType _originatingService, uint64_t _priority) noexcept : Event(_id, _originatingService, _priority) {}
    ~CustomDoWorkEvent() final = default;

    [[nodiscard]] std::string_view get_name() const noexcept final {
        return NAME;
    }
    [[nodiscard]] NameHashType get_type() const noexcept final {
        return TYPE;
    }

    static constexpr NameHashType TYPE = typeNameHash<CustomDoWorkEvent>();
    static constexpr std::string_view NAME = typeName<CustomDoWorkEvent>();
};

struct IAsyncBroadcastService {
    [[nodiscard]] virtual uint64_t getFinishedCalls() noexcept = 0;
    [[nodiscard]] virtual uint64_t getFinishedPosts() noexcept = 0;
    [[nodiscard]] virtual Task<void> postEvent() noexcept = 0;
protected:
    ~IAsyncBroadcastService() = default;
};

struct AsyncBroadcastService final : public IAsyncBroadcastService, public AdvancedService<AsyncBroadcastService> {
    AsyncBroadcastService() = default;
    ~AsyncBroadcastService() final = default;
    Task<tl::expected<void, Ichor::StartError>> start() final {
        _handler = GetThreadLocalManager().registerEventHandler<CustomDoWorkEvent>(this, this);

        co_return {};
    }

    Task<void> stop() final {
        _handler.reset();

        co_return;
    }

    [[nodiscard]] uint64_t getFinishedCalls() noexcept final {
        return _finishedCalls;
    }

    [[nodiscard]] uint64_t getFinishedPosts() noexcept final {
        return _finishedPosts;
    }

    [[nodiscard]] Task<void> postEvent() noexcept final {
        co_await GetThreadLocalManager().pushPrioritisedEventAsync<CustomDoWorkEvent>(getServiceId(), 100, false);
        _finishedPosts++;
        co_return;
    }

    AsyncGenerator<IchorBehaviour> handleEvent(CustomDoWorkEvent const &) {
        co_await *_evt;

        _finishedCalls++;

        co_return {};
    }

    EventHandlerRegistration _handler{};
    uint64_t _finishedCalls{};
    uint64_t _finishedPosts{};
};
