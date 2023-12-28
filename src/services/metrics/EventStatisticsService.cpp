#include <ichor/services/metrics/EventStatisticsService.h>
#include <ichor/DependencyManager.h>
#include <numeric>

Ichor::EventStatisticsService::EventStatisticsService(DependencyRegister &reg, Properties props) : AdvancedService<EventStatisticsService>(std::move(props)) {
    reg.registerDependency<ITimerFactory>(this, DependencyFlags::REQUIRED);
    reg.registerDependency<ILogger>(this, DependencyFlags::REQUIRED);
}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::EventStatisticsService::start() {
    if(getProperties().contains("ShowStatisticsOnStop")) {
        _showStatisticsOnStop = Ichor::any_cast<bool>(getProperties()["ShowStatisticsOnStop"]);
    }

    if(getProperties().contains("AveragingIntervalMs")) {
        _averagingIntervalMs = Ichor::any_cast<uint64_t>(getProperties()["AveragingIntervalMs"]);
    } else {
        _averagingIntervalMs = 500;
    }

    auto &timer = _timerFactory->createTimer();
    timer.setChronoInterval(std::chrono::milliseconds(_averagingIntervalMs));

    timer.setCallbackAsync([this]() -> AsyncGenerator<IchorBehaviour> {
        return handleEvent();
    });

    _interceptorRegistration = GetThreadLocalManager().registerEventInterceptor<Event>(this, this);
    timer.startTimer();

    co_return {};
}

Ichor::Task<void> Ichor::EventStatisticsService::stop() {
    _interceptorRegistration.reset();

    if(_showStatisticsOnStop) {
        // handle last bit of stored statistics by emulating a handleEvent call
        auto gen = handleEvent();
        auto it = gen.begin();
        while(!gen.done() && !it.get_finished()) {
            it = gen.begin();
        }

        for(auto &[key, statistics] : _averagedStatistics) {
            if(statistics.empty()) {
                continue;
            }

            auto min = std::min_element(begin(statistics), end(statistics), [](const AveragedStatisticEntry &a, const AveragedStatisticEntry &b){return a.minProcessingTimeRequired < b.minProcessingTimeRequired; })->minProcessingTimeRequired;
            auto max = std::max_element(begin(statistics), end(statistics), [](const AveragedStatisticEntry &a, const AveragedStatisticEntry &b){return a.maxProcessingTimeRequired < b.maxProcessingTimeRequired; })->maxProcessingTimeRequired;
            auto avg = std::accumulate(begin(statistics), end(statistics), 0L, [](int64_t i, const AveragedStatisticEntry &entry) -> int64_t { return i + entry.avgProcessingTimeRequired; }) / static_cast<int64_t>(statistics.size());
            auto occ = std::accumulate(begin(statistics), end(statistics), 0L, [](int64_t i, const AveragedStatisticEntry &entry){ return i + entry.occurrences; });

            ICHOR_LOG_ERROR(_logger, "Dm {:L} Event type {} occurred {:L} times, min/max/avg processing: {:L}/{:L}/{:L} ns", GetThreadLocalManager().getId(), _eventTypeToNameMapper[key], occ, min, max, avg);
        }
    }

    co_return;
}

bool Ichor::EventStatisticsService::preInterceptEvent(Event const &evt) {
    _startProcessingTimestamp = std::chrono::steady_clock::now();

    if(!_eventTypeToNameMapper.contains(evt.type)) {
        _eventTypeToNameMapper.emplace(evt.type, evt.name);
    }

    return (bool)AllowOthersHandling;
}

void Ichor::EventStatisticsService::postInterceptEvent(Event const &evt, bool processed) {
    if(!processed) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto processingTime = now - _startProcessingTimestamp;
    auto statistics = _recentEventStatistics.find(evt.type);

    if(statistics == end(_recentEventStatistics)) {
        _recentEventStatistics.emplace(evt.type, std::vector<StatisticEntry>{{StatisticEntry{
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(),
            std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count()}}});
    } else {
        statistics->second.emplace_back(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(),
                std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count());
    }
}

void Ichor::EventStatisticsService::addDependencyInstance(ILogger &logger, IService &) {
    _logger = &logger;
}

void Ichor::EventStatisticsService::removeDependencyInstance(ILogger &, IService&) {
    _logger = nullptr;
}

void Ichor::EventStatisticsService::addDependencyInstance(ITimerFactory &factory, IService &) {
    _timerFactory = &factory;
}

void Ichor::EventStatisticsService::removeDependencyInstance(ITimerFactory &factory, IService&) {
    _timerFactory = nullptr;
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::EventStatisticsService::handleEvent() {
    int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    decltype(_recentEventStatistics) newVec{};
    newVec.swap(_recentEventStatistics);
    _recentEventStatistics.clear();

    for(auto &[key, statistics] : newVec) {
        auto avgStatistics = _averagedStatistics.find(key);

        if(statistics.empty()) {
            continue;
        }

        auto min = std::min_element(begin(statistics), end(statistics), [](const StatisticEntry &a, const StatisticEntry &b){return a.processingTimeRequired < b.processingTimeRequired; })->processingTimeRequired;
        auto max = std::max_element(begin(statistics), end(statistics), [](const StatisticEntry &a, const StatisticEntry &b){return a.processingTimeRequired < b.processingTimeRequired; })->processingTimeRequired;
        auto avg = std::accumulate(begin(statistics), end(statistics), 0L, [](int64_t i, const StatisticEntry &entry){ return i + entry.processingTimeRequired; }) / static_cast<int64_t>(statistics.size());

        if(avgStatistics == end(_averagedStatistics)) {
            _averagedStatistics.emplace(key, std::vector<AveragedStatisticEntry>{{AveragedStatisticEntry{now, min, max, avg, static_cast<int64_t>(statistics.size())}}});
        } else {
            avgStatistics->second.emplace_back(now, min, max, avg, statistics.size());
        }

        co_yield {};
    }
    co_return {};
}

const Ichor::unordered_map<uint64_t, std::vector<Ichor::StatisticEntry>> &Ichor::EventStatisticsService::getRecentStatistics() const noexcept {
    return _recentEventStatistics;
}

const Ichor::unordered_map<uint64_t, std::vector<Ichor::AveragedStatisticEntry>> &Ichor::EventStatisticsService::getAverageStatistics() const noexcept {
    return _averagedStatistics;
}
