#pragma once

#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/event_queues/IIOUringQueue.h>
#include <ichor/ichor_liburing.h>

namespace Ichor {
    struct IOUringSleepService final : public AdvancedService<IOUringSleepService> {
        IOUringSleepService(DependencyRegister &reg, Properties props) : AdvancedService<IOUringSleepService>(std::move(props)) {
            reg.registerDependency<IIOUringQueue>(this, DependencyFlags::REQUIRED);
        }

        Task<tl::expected<void, Ichor::StartError>> start() final {
            {
                AsyncManualResetEvent evt;
                int res{};
                auto *sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
                    INTERNAL_IO_DEBUG("timeout res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
                    res = cqe->res;
                    evt.set();
                });
                timespec.tv_nsec = 50'000'000;
                io_uring_prep_timeout(sqe, &timespec, 0, 0);

                co_await evt;
            }

            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
            co_return {};
        }

        Task<void> stop() final {
            {
                AsyncManualResetEvent evt;
                int res{};
                auto *sqe = _q->getSqeWithData(this, [&evt, &res](io_uring_cqe *cqe) {
                    INTERNAL_IO_DEBUG("timeout res: {} {}", cqe->res, cqe->res < 0 ? strerror(-cqe->res) : "");
                    res = cqe->res;
                    evt.set();
                });
                timespec.tv_nsec = 50'000'000;
                io_uring_prep_timeout(sqe, &timespec, 0, 0);

                co_await evt;
            }
            co_return;
        }

        void addDependencyInstance(IIOUringQueue& q, IService&) {
            _q = &q;
        }

        void removeDependencyInstance(IIOUringQueue&, IService&) {
            _q = nullptr;
        }

        IIOUringQueue *_q;
        __kernel_timespec timespec{};
    };
}
