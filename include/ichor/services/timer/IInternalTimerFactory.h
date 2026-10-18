#pragma once

#include <ichor/coroutines/Task.h>

namespace Ichor::v1::Detail {
    struct InternalTimerFactory {
        virtual Task<void> stopAllTimers() noexcept = 0;

    protected:
        ~InternalTimerFactory() = default;
    };
    }

