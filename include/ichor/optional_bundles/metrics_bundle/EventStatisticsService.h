#pragma once

#include <unordered_map>
#include <ichor/Service.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <chrono>

namespace Ichor {

    struct StatisticEntry {
        StatisticEntry() = default;
        StatisticEntry(int64_t _timestamp, int64_t _processingTimeRequired) : timestamp(_timestamp), processingTimeRequired(_processingTimeRequired) {}
        int64_t timestamp{};
        int64_t processingTimeRequired{};
    };

    struct AveragedStatisticEntry {
        AveragedStatisticEntry() = default;
        AveragedStatisticEntry(int64_t _timestamp, int64_t _minProcessingTimeRequired, int64_t _maxProcessingTimeRequired, int64_t _avgProcessingTimeRequired, uint64_t _occurances) :
            timestamp(_timestamp), minProcessingTimeRequired(_minProcessingTimeRequired), maxProcessingTimeRequired(_maxProcessingTimeRequired), avgProcessingTimeRequired(_avgProcessingTimeRequired),
            occurances(_occurances) {}
        int64_t timestamp{};
        int64_t minProcessingTimeRequired{};
        int64_t maxProcessingTimeRequired{};
        int64_t avgProcessingTimeRequired{};
        uint64_t occurances{};
    };

    class IEventStatisticsService : virtual public IService {
    public:
        ~IEventStatisticsService() override = default;

        [[nodiscard]] virtual const std::unordered_map<uint64_t, std::vector<StatisticEntry>>& getRecentStatistics() const noexcept = 0;
        [[nodiscard]] virtual const std::unordered_map<uint64_t, std::vector<AveragedStatisticEntry>>& getAverageStatistics() const noexcept = 0;
    };

    class EventStatisticsService final : public IEventStatisticsService, public Service<EventStatisticsService> {
    public:
        ~EventStatisticsService() final = default;

        bool preInterceptEvent(Event const * const evt);
        bool postInterceptEvent(Event const * const evt, bool processed);

        Generator<bool> handleEvent(TimerEvent const * const evt);

        bool start() final;
        bool stop() final;

        const std::unordered_map<uint64_t, std::vector<StatisticEntry>>& getRecentStatistics() const noexcept final;
        const std::unordered_map<uint64_t, std::vector<AveragedStatisticEntry>>& getAverageStatistics() const noexcept final;
    private:
        std::unordered_map<uint64_t, std::vector<StatisticEntry>> _recentEventStatistics;
        std::unordered_map<uint64_t, std::vector<AveragedStatisticEntry>> _averagedStatistics;
        std::unordered_map<uint64_t, std::string_view> _eventTypeToNameMapper;
        std::chrono::time_point<std::chrono::steady_clock> _startProcessingTimestamp{};
        bool _showStatisticsOnStop{false};
        uint64_t _averagingIntervalMs{5000};
        std::unique_ptr<EventHandlerRegistration, Deleter> _timerEventRegistration{nullptr};
        std::unique_ptr<EventInterceptorRegistration, Deleter> _interceptorRegistration{nullptr};
    };
}