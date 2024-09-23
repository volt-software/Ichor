#pragma once

namespace Ichor::Detail {
    struct InternalTimerFactory {
        virtual void stopAllTimers() noexcept = 0;

    protected:
        ~InternalTimerFactory() = default;
    };
    }

