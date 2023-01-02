#pragma once

#include <unordered_map>
#include <ichor/dependency_management/Service.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/TimerService.h>
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
        AveragedStatisticEntry(int64_t _timestamp, int64_t _minProcessingTimeRequired, int64_t _maxProcessingTimeRequired, int64_t _avgProcessingTimeRequired, int64_t _occurrences) :
            timestamp(_timestamp), minProcessingTimeRequired(_minProcessingTimeRequired), maxProcessingTimeRequired(_maxProcessingTimeRequired), avgProcessingTimeRequired(_avgProcessingTimeRequired),
            occurrences(_occurrences) {}
        int64_t timestamp{};
        int64_t minProcessingTimeRequired{};
        int64_t maxProcessingTimeRequired{};
        int64_t avgProcessingTimeRequired{};
        int64_t occurrences{};
    };

    class IEventStatisticsService {
    public:
        [[nodiscard]] virtual const unordered_map<uint64_t, std::vector<StatisticEntry>>& getRecentStatistics() const noexcept = 0;
        [[nodiscard]] virtual const unordered_map<uint64_t, std::vector<AveragedStatisticEntry>>& getAverageStatistics() const noexcept = 0;

    protected:
        ~IEventStatisticsService() = default;
    };

    class EventStatisticsService final : public IEventStatisticsService, public Service<EventStatisticsService> {
    public:
        EventStatisticsService() = default;
        ~EventStatisticsService() final = default;

        const unordered_map<uint64_t, std::vector<StatisticEntry>>& getRecentStatistics() const noexcept final;
        const unordered_map<uint64_t, std::vector<AveragedStatisticEntry>>& getAverageStatistics() const noexcept final;
    private:
        bool preInterceptEvent(Event const &evt);
        void postInterceptEvent(Event const &evt, bool processed);

        AsyncGenerator<IchorBehaviour> handleEvent(DependencyManager &dm);

        AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final;
        AsyncGenerator<void> stop() final;

        friend DependencyRegister;
        friend DependencyManager;

        unordered_map<uint64_t, std::vector<StatisticEntry>> _recentEventStatistics;
        unordered_map<uint64_t, std::vector<AveragedStatisticEntry>> _averagedStatistics;
        unordered_map<uint64_t, std::string_view> _eventTypeToNameMapper;
        std::chrono::time_point<std::chrono::steady_clock> _startProcessingTimestamp{};
        bool _showStatisticsOnStop{false};
        uint64_t _averagingIntervalMs{5000};
        EventInterceptorRegistration _interceptorRegistration{};
    };
}
