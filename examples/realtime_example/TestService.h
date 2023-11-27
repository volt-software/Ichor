#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include "OptionalService.h"
#include "gsm_enc/gsm_enc.h"
#include <array>
#include <numeric>

using namespace Ichor;

#define ITERATIONS 5'000
#ifdef ICHOR_AARCH64
#define MAXIMUM_DURATION_USEC 1500
#else
#define MAXIMUM_DURATION_USEC 500
#endif

struct ExecuteTaskEvent final : public Event {
    ExecuteTaskEvent(uint64_t _id, uint64_t _originatingService, uint64_t _priority) noexcept : Event(TYPE, NAME, _id, _originatingService, _priority) {}
    ~ExecuteTaskEvent() final = default;

    static constexpr uint64_t TYPE = typeNameHash<ExecuteTaskEvent>();
    static constexpr std::string_view NAME = typeName<ExecuteTaskEvent>();
};

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IOptionalService>(this, false);
    }
    ~TestService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        _started = true;
        _eventHandlerRegistration = GetThreadLocalManager().registerEventHandler<ExecuteTaskEvent>(this, this);
        if(_injectionCount == 2) {
            enqueueWorkload();
        }
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
        _eventHandlerRegistration.reset();
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &isvc) {
        _logger = &logger;

        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", isvc.getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger&, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(IOptionalService&, IService &isvc) {
        ICHOR_LOG_INFO(_logger, "Inserted IOptionalService svcid {}", isvc.getServiceId());

        _injectionCount++;
        if(_started && _injectionCount == 2) {
            enqueueWorkload();
        }
    }

    void removeDependencyInstance(IOptionalService&, IService&) {
    }

    void enqueueWorkload() {
        GetThreadLocalEventQueue().pushPrioritisedEvent<ExecuteTaskEvent>(getServiceId(), 10u);
    }

    AsyncGenerator<IchorBehaviour> handleEvent(ExecuteTaskEvent const &evt) {
        auto start = std::chrono::high_resolution_clock::now();
        run_gsm_enc_bench(); // run gsm_bench from TACLe bench
        auto endtime = std::chrono::high_resolution_clock::now();
        _executionTimes[_finishedWorkloads++] = std::chrono::duration_cast<std::chrono::microseconds>(endtime - start).count();

        if(_executionTimes[_finishedWorkloads - 1] > MAXIMUM_DURATION_USEC) {
            fmt::print("duration of run {:L} is {:L} µs which exceeded maximum of {:L} µs\n", _finishedWorkloads - 1, _executionTimes[_finishedWorkloads - 1], MAXIMUM_DURATION_USEC);
        }

        if(_finishedWorkloads == ITERATIONS) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());

            long min = *std::min_element(begin(_executionTimes), end(_executionTimes), [](long &a, long &b){return a < b; });
            long max = *std::max_element(begin(_executionTimes), end(_executionTimes), [](long &a, long &b){return a < b; });
            long avg = std::accumulate(begin(_executionTimes), end(_executionTimes), 0L, [](long i, long &entry){ return i + entry; }) / static_cast<long>(_executionTimes.size());

            fmt::print("duration min/max/avg: {:L}/{:L}/{:L} µs\n", min, max, avg);
        } else {
            GetThreadLocalEventQueue().pushPrioritisedEvent<ExecuteTaskEvent>(getServiceId(), 10u);
        }

        co_return {};
    }

    friend DependencyRegister;
    friend DependencyManager;

    ILogger *_logger{};
    bool _started{false};
    uint64_t _injectionCount{0};
    uint64_t _finishedWorkloads{0};
    std::array<long, ITERATIONS> _executionTimes{};
    EventHandlerRegistration _eventHandlerRegistration{};
};
