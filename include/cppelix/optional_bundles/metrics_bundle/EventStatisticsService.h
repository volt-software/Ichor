#pragma once

#include <unordered_map>
#include <cppelix/Service.h>
#include <cppelix/optional_bundles/logging_bundle/Logger.h>
#include <cppelix/optional_bundles/timer_bundle/TimerService.h>

namespace Cppelix {

    struct StatisticEntry {
        StatisticEntry() = default;
        StatisticEntry(uint64_t _timestamp, uint64_t _processingTimeRequired) : timestamp(_timestamp), processingTimeRequired(_processingTimeRequired) {}
        uint64_t timestamp;
        uint64_t processingTimeRequired;
    };

    struct AveragedStatisticEntry {
        AveragedStatisticEntry() = default;
        AveragedStatisticEntry(uint64_t _timestamp, uint64_t _minProcessingTimeRequired, uint64_t _maxProcessingTimeRequired, uint64_t _avgProcessingTimeRequired, uint64_t _occurances) :
            timestamp(_timestamp), minProcessingTimeRequired(_minProcessingTimeRequired), maxProcessingTimeRequired(_maxProcessingTimeRequired), avgProcessingTimeRequired(_avgProcessingTimeRequired),
            occurances(_occurances) {}
        uint64_t timestamp;
        uint64_t minProcessingTimeRequired;
        uint64_t maxProcessingTimeRequired;
        uint64_t avgProcessingTimeRequired;
        uint64_t occurances;
    };

    class IEventStatisticsService : public virtual IService {
    public:
        static constexpr InterfaceVersion version = InterfaceVersion{1, 0, 0};

        ~IEventStatisticsService() override = default;

        virtual const std::unordered_map<std::string_view, std::vector<StatisticEntry>>& getRecentStatistics() = 0;
        virtual const std::unordered_map<std::string_view, std::vector<AveragedStatisticEntry>>& getAverageStatistics() = 0;
    };

    class EventStatisticsService final : public IEventStatisticsService, public Service {
    public:
        EventStatisticsService(DependencyRegister &reg, CppelixProperties props);
        ~EventStatisticsService() final = default;

        void addDependencyInstance(ILogger *logger);
        void removeDependencyInstance(ILogger *logger);

        bool preInterceptEvent(Event const * const evt);
        bool postInterceptEvent(Event const * const evt, bool processed);

        Generator<bool> handleEvent(TimerEvent const * const evt);

        bool start() final;
        bool stop() final;

        const std::unordered_map<std::string_view, std::vector<StatisticEntry>>& getRecentStatistics() override;
        const std::unordered_map<std::string_view, std::vector<AveragedStatisticEntry>>& getAverageStatistics() override;
    private:
        std::unordered_map<std::string_view, std::vector<StatisticEntry>> _recentEventStatistics{};
        std::unordered_map<std::string_view, std::vector<AveragedStatisticEntry>> _averagedStatistics{};
        uint64_t _startProcessingTimestamp{};
        bool _showStatisticsOnStop{false};
        uint64_t _averagingIntervalMs{5000};
        std::unique_ptr<EventHandlerRegistration> _timerEventRegistration{nullptr};
        std::unique_ptr<EventInterceptorRegistration> _interceptorRegistration{nullptr};
        ILogger *_logger{nullptr};
    };
}