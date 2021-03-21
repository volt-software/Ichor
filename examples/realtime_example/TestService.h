#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/Service.h>
#include <ichor/LifecycleManager.h>
#include "OptionalService.h"
#include "gsm_enc/gsm_enc.h"

using namespace Ichor;

#define ITERATIONS 20'000
#define MAXIMUM_DURATION_USEC 2'000

struct ExecuteTaskEvent final : public Event {
    ExecuteTaskEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority) {}
    ~ExecuteTaskEvent() final = default;

    static constexpr uint64_t TYPE = typeNameHash<ExecuteTaskEvent>();
    static constexpr std::string_view NAME = typeName<ExecuteTaskEvent>();
};

class TestService final : public Service<TestService> {
public:
    TestService(DependencyRegister &reg, IchorProperties props, DependencyManager *mng) : Service(std::move(props), mng) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IOptionalService>(this, false);
    }
    ~TestService() final = default;
    bool start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        _started = true;
        _eventHandlerRegistration = getManager()->registerEventHandler<ExecuteTaskEvent>(this);
        if(_injectionCount == 2) {
            enqueueWorkload();
        }
        return true;
    }

    bool stop() final {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
        _eventHandlerRegistration.reset();
        return true;
    }

    void addDependencyInstance(ILogger *logger) {
        _logger = logger;

        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", logger->getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger *logger) {
        _logger = nullptr;
    }

    void addDependencyInstance(IOptionalService *svc) {
        ICHOR_LOG_INFO(_logger, "Inserted IOptionalService svcid {}", svc->getServiceId());

        _injectionCount++;
        if(_started && _injectionCount == 2) {
            enqueueWorkload();
        }
    }

    void removeDependencyInstance(IOptionalService *) {
    }

    void enqueueWorkload() {
        getManager()->pushPrioritisedEvent<ExecuteTaskEvent>(getServiceId(), 10);
    }

    Generator<bool> handleEvent(ExecuteTaskEvent const * const evt) {
        auto start = std::chrono::high_resolution_clock::now();
        run_gsm_enc_bench(); // run gsm_bench from TACLe bench
        auto endtime = std::chrono::high_resolution_clock::now();
        _executionTimes[_finishedWorkloads++] = std::chrono::duration_cast<std::chrono::microseconds>(endtime - start).count();

        if(_executionTimes[_finishedWorkloads - 1] > MAXIMUM_DURATION_USEC) {
            fmt::print("duration of run {:L} is {:L} µs which exceeded maximum of {:L} µs\n", _finishedWorkloads - 1, _executionTimes[_finishedWorkloads - 1], MAXIMUM_DURATION_USEC);
        }

        if(_finishedWorkloads == ITERATIONS) {
            getManager()->pushEvent<QuitEvent>(getServiceId());

            long min = *std::min_element(begin(_executionTimes), end(_executionTimes), [](long &a, long &b){return a < b; });
            long max = *std::max_element(begin(_executionTimes), end(_executionTimes), [](long &a, long &b){return a < b; });
            long avg = std::accumulate(begin(_executionTimes), end(_executionTimes), 0L, [](long i, long &entry){ return i + entry; }) / static_cast<long>(_executionTimes.size());
            
            fmt::print("duration min/max/avg: {:L}/{:L}/{:L} µs\n", min, max, avg);
        } else {
            getManager()->pushPrioritisedEvent<ExecuteTaskEvent>(getServiceId(), 10);
        }

        co_return (bool)PreventOthersHandling;
    }

private:
    ILogger *_logger{nullptr};
    bool _started{false};
    int _injectionCount{0};
    int _finishedWorkloads{0};
    std::array<long, ITERATIONS> _executionTimes{};
    std::unique_ptr<EventHandlerRegistration, Deleter> _eventHandlerRegistration{nullptr};
};