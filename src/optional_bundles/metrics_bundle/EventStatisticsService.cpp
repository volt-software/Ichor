#include <ichor/optional_bundles/metrics_bundle/EventStatisticsService.h>

bool Ichor::EventStatisticsService::start() {
    if(getProperties()->contains("ShowStatisticsOnStop")) {
        _showStatisticsOnStop = Ichor::any_cast<bool>(getProperties()->operator[]("ShowStatisticsOnStop"));
    }

    if(getProperties()->contains("AveragingIntervalMs")) {
        _averagingIntervalMs = Ichor::any_cast<uint64_t>(getProperties()->operator[]("AveragingIntervalMs"));
    } else {
        _averagingIntervalMs = 500;
    }

    auto _timerManager = getManager()->createServiceManager<Timer, ITimer>();
    _timerManager->setChronoInterval(std::chrono::milliseconds(_averagingIntervalMs));
    _timerEventRegistration = getManager()->registerEventHandler<TimerEvent>(this, _timerManager->getServiceId());
    _interceptorRegistration = getManager()->registerEventInterceptor<Event>(this);
    _timerManager->startTimer();

    return true;
}

bool Ichor::EventStatisticsService::stop() {
    _timerEventRegistration.reset();
    _interceptorRegistration.reset();

    if(_showStatisticsOnStop) {
        // handle last bit of stored statistics by emulating a handleEvent call
        auto gen = handleEvent(nullptr);
        auto it = gen.begin();
        while(it != gen.end()) {
            it = gen.begin();
        }

        for(auto &[key, statistics] : _averagedStatistics) {
            if(statistics.empty()) {
                continue;
            }

            auto min = std::min_element(begin(statistics), end(statistics), [](const AveragedStatisticEntry &a, const AveragedStatisticEntry &b){return a.minProcessingTimeRequired < b.minProcessingTimeRequired; })->minProcessingTimeRequired;
            auto max = std::max_element(begin(statistics), end(statistics), [](const AveragedStatisticEntry &a, const AveragedStatisticEntry &b){return a.maxProcessingTimeRequired < b.maxProcessingTimeRequired; })->maxProcessingTimeRequired;
            auto avg = std::accumulate(begin(statistics), end(statistics), 0L, [](int64_t i, const AveragedStatisticEntry &entry){ return i + entry.avgProcessingTimeRequired; }) / statistics.size();
            auto occ = std::accumulate(begin(statistics), end(statistics), 0L, [](int64_t i, const AveragedStatisticEntry &entry){ return i + entry.occurances; });

            ICHOR_LOG_ERROR(getManager()->getLogger(), "Dm {:L} Event type {} occurred {:L} times, min/max/avg processing: {:L}/{:L}/{:L} ns", getManager()->getId(), _eventTypeToNameMapper[key], occ, min, max, avg);
        }
    }

    return true;
}

bool Ichor::EventStatisticsService::preInterceptEvent(const Event *const evt) {
    _startProcessingTimestamp = std::chrono::steady_clock::now();

    if(!_eventTypeToNameMapper.contains(evt->type)) {
        _eventTypeToNameMapper.emplace(evt->type, evt->name);
    }

    return (bool)AllowOthersHandling;
}

bool Ichor::EventStatisticsService::postInterceptEvent(const Event *const evt, bool processed) {
    if(!processed) {
        return (bool)AllowOthersHandling;
    }

    auto now = std::chrono::steady_clock::now();
    auto processingTime = now - _startProcessingTimestamp;
    auto statistics = _recentEventStatistics.find(evt->type);

    if(statistics == end(_recentEventStatistics)) {
        _recentEventStatistics.emplace(evt->type, std::vector<StatisticEntry>{{StatisticEntry{
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(),
            std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count()}}});
    } else {
        statistics->second.emplace_back(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(),
                std::chrono::duration_cast<std::chrono::nanoseconds>(processingTime).count());
    }

    return (bool)AllowOthersHandling;
}

Ichor::Generator<bool> Ichor::EventStatisticsService::handleEvent([[maybe_unused]] const TimerEvent *const evt) {
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
            _averagedStatistics.emplace(key, std::vector<AveragedStatisticEntry>{{AveragedStatisticEntry{now, min, max, avg, statistics.size()}}});
        } else {
            avgStatistics->second.emplace_back(now, min, max, avg, statistics.size());
        }

        co_yield (bool)Ichor::PreventOthersHandling;
    }
    co_return (bool)Ichor::PreventOthersHandling;
}

const std::unordered_map<uint64_t, std::vector<Ichor::StatisticEntry>> &Ichor::EventStatisticsService::getRecentStatistics() const noexcept {
    return _recentEventStatistics;
}

const std::unordered_map<uint64_t, std::vector<Ichor::AveragedStatisticEntry>> &Ichor::EventStatisticsService::getAverageStatistics() const noexcept {
    return _averagedStatistics;
}
