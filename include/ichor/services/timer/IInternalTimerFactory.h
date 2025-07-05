#pragma once

#include <ichor/coroutines/Task.h>

namespace Ichor::Detail::v1 {
    struct InternalTimerFactory {
        virtual Task<void> stopAllTimers() noexcept = 0;

    protected:
        ~InternalTimerFactory() = default;
    };
    }

