#pragma once

#include <ichor/event_queues/IEventQueue.h>

struct QueueMock final : public IEventQueue, public AdvancedService<QueueMock> {
    QueueMock(DependencyRegister &reg, Properties props) : AdvancedService<QueueMock>(std::move(props)) {}

    ~QueueMock() final = default;
    [[nodiscard]] bool empty() const override {
        return events.empty();
    }
    [[nodiscard]] uint64_t size() const override {
        return events.size();
    }
    [[nodiscard]] bool is_running() const noexcept override {
        return isRunning;
    }
    [[nodiscard]] NameHashType get_queue_name_hash() const noexcept override {
        return typeNameHash<QueueMock>();
    }

    bool start(bool captureSigInt) override {
        return false;
    }

    [[nodiscard]] bool shouldQuit() override {
        return false;
    }
    void quit() override {
    }

    void pushEventInternal(uint64_t priority, std::unique_ptr<Event> &&event) override {
        events.emplace_back(std::move(event));
    }

    std::vector<std::unique_ptr<Event>> events;
    bool isRunning{true};
};
