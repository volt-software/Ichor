#pragma once

using namespace Ichor;

struct NoCopyNoMove {
    NoCopyNoMove() {
        countConstructed++;
    }
    ~NoCopyNoMove() noexcept {
        countDestructed++;
    }
    NoCopyNoMove(NoCopyNoMove const &) = delete;
    NoCopyNoMove(NoCopyNoMove&& o) noexcept = delete;
    NoCopyNoMove& operator=(NoCopyNoMove const &) = delete;
    NoCopyNoMove& operator=(NoCopyNoMove&& o) noexcept = delete;

    static uint64_t countConstructed;
    static uint64_t countDestructed;
};

extern AsyncReturningManualResetEvent<int> _evtInt;
extern AsyncReturningManualResetEvent<NoCopyNoMove> _evtNoCopyNoMove;

struct IAwaitReturningManualResetService {
    virtual Task<int> AwaitInt() = 0;
    virtual Task<NoCopyNoMove const &> AwaitNoCopyNoMove() = 0;

protected:
    ~IAwaitReturningManualResetService() = default;
};

struct AwaitReturningManualResetService final : public IAwaitReturningManualResetService, public AdvancedService<AwaitReturningManualResetService> {
    AwaitReturningManualResetService() = default;
    ~AwaitReturningManualResetService() final = default;

    Task<int> AwaitInt() final {
        co_return co_await _evtInt;
    }

    Task<NoCopyNoMove const &> AwaitNoCopyNoMove() final {
        co_return co_await _evtNoCopyNoMove;
    }
};
