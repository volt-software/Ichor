#include <cppelix/optional_bundles/metrics_bundle/EventStatisticsService.h>

Cppelix::EventStatisticsService::EventStatisticsService(Cppelix::DependencyRegister &reg, Cppelix::CppelixProperties props) : Service(std::move(props)) {
    reg.registerDependency<ILogger>(this, true);
}

bool Cppelix::EventStatisticsService::start() {
    if(getProperties()->contains("ShowStatisticsOnStop")) {
        _showStatisticsOnStop = std::any_cast<bool>(getProperties()->operator[]("ShowStatisticsOnStop"));
    }

    if(getProperties()->contains("AveragingIntervalMs")) {
        _averagingIntervalMs = std::any_cast<uint64_t>(getProperties()->operator[]("AveragingIntervalMs"));
    } else {
        _averagingIntervalMs = 5000;
    }

    auto _timerManager = getManager()->createServiceManager<Timer, ITimer>();
    _timerManager->setChronoInterval(std::chrono::milliseconds(_averagingIntervalMs));
    _timerEventRegistration = getManager()->registerEventHandler<TimerEvent>(getServiceId(), this, _timerManager->getServiceId());
    _timerManager->startTimer();

    _interceptorRegistration = getManager()->registerEventInterceptor<Event>(getServiceId(), this);

    return true;
}

bool Cppelix::EventStatisticsService::stop() {
    _timerEventRegistration = nullptr;

    if(_showStatisticsOnStop) {
        for(auto &[key, statistics] : _averagedStatistics) {
            if(statistics.empty()) {
                continue;
            }

            auto min = std::min_element(begin(statistics), end(statistics), [](const AveragedStatisticEntry &a, const AveragedStatisticEntry &b){return a.minProcessingTimeRequired < b.minProcessingTimeRequired; })->minProcessingTimeRequired;
            auto max = std::max_element(begin(statistics), end(statistics), [](const AveragedStatisticEntry &a, const AveragedStatisticEntry &b){return a.maxProcessingTimeRequired < b.maxProcessingTimeRequired; })->maxProcessingTimeRequired;
            auto avg = std::accumulate(begin(statistics), end(statistics), 0UL, [](uint64_t i, const AveragedStatisticEntry &entry){ return i + entry.avgProcessingTimeRequired; }) / statistics.size();
            auto occ = std::accumulate(begin(statistics), end(statistics), 0UL, [](uint64_t i, const AveragedStatisticEntry &entry){ return i + entry.occurances; });

            LOG_INFO(_logger, "Event type {} occurred {} times, min/max/avg processing: {}/{}/{} µs", key, occ, min, max, avg);
        }
    }

    return true;
}

void Cppelix::EventStatisticsService::addDependencyInstance(Cppelix::ILogger *logger) {
    _logger = logger;
}

void Cppelix::EventStatisticsService::removeDependencyInstance(Cppelix::ILogger *logger) {
    _logger = nullptr;
}

bool Cppelix::EventStatisticsService::preInterceptEvent(const Cppelix::Event *const evt) {
    _startProcessingTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return (bool)AllowOthersHandling;
}

bool Cppelix::EventStatisticsService::postInterceptEvent(const Cppelix::Event *const evt, bool processed) {
    if(!processed) {
        return (bool)AllowOthersHandling;
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto processingTime = now - _startProcessingTimestamp;
    auto statistics = _recentEventStatistics.find(evt->name);

    if(statistics == end(_recentEventStatistics)) {
        _recentEventStatistics.emplace(evt->name, std::vector<StatisticEntry>{StatisticEntry{now, processingTime}});
    } else {
        statistics->second.emplace_back(now, processingTime);
    }

    return (bool)AllowOthersHandling;
}

Cppelix::Generator<bool> Cppelix::EventStatisticsService::handleEvent(const Cppelix::TimerEvent *const evt) {
    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for(auto &[key, statistics] : _recentEventStatistics) {
        auto avgStatistics = _averagedStatistics.find(key);

        if(statistics.empty()) {
            if(avgStatistics == end(_averagedStatistics)) {
                _averagedStatistics.emplace(key, std::vector<AveragedStatisticEntry>{AveragedStatisticEntry{now, 0, 0, 0, 0}});
            } else {
                avgStatistics->second.emplace_back(now, 0, 0, 0, 0);
            }
            _averagedStatistics.emplace(key, std::vector<AveragedStatisticEntry>{AveragedStatisticEntry{now, 0, 0, 0, 0}});
            continue;
        }

        auto min = std::min_element(begin(statistics), end(statistics), [](const StatisticEntry &a, const StatisticEntry &b){return a.processingTimeRequired < b.processingTimeRequired; })->processingTimeRequired;
        auto max = std::max_element(begin(statistics), end(statistics), [](const StatisticEntry &a, const StatisticEntry &b){return a.processingTimeRequired < b.processingTimeRequired; })->processingTimeRequired;
        auto avg = std::accumulate(begin(statistics), end(statistics), 0UL, [](uint64_t i, const StatisticEntry &entry){ return i + entry.processingTimeRequired; }) / statistics.size();

        if(avgStatistics == end(_averagedStatistics)) {
            _averagedStatistics.emplace(key, std::vector<AveragedStatisticEntry>{AveragedStatisticEntry{now, min, max, avg, statistics.size()}});
        } else {
            avgStatistics->second.emplace_back(now, min, max, avg, statistics.size());
        }

        co_yield (bool)Cppelix::PreventOthersHandling;
    }
    co_return (bool)Cppelix::PreventOthersHandling;
}

const std::unordered_map<std::string_view, std::vector<Cppelix::StatisticEntry>> &Cppelix::EventStatisticsService::getRecentStatistics() {
    return _recentEventStatistics;
}

const std::unordered_map<std::string_view, std::vector<Cppelix::AveragedStatisticEntry>> &Cppelix::EventStatisticsService::getAverageStatistics() {
    return _averagedStatistics;
}
