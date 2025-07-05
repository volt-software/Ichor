#pragma once

#include <ichor/services/io/IAsyncFileIO.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/stl/ErrnoUtils.h>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

namespace Ichor::v1 {
    struct io_operation_submission final {
        Ichor::AsyncManualResetEvent evt;
        tl::expected<void, Ichor::v1::IOError> result;
        std::function<void(decltype(result)&)> fn;
    };

    class SharedOverThreadsAsyncFileIO final : public IAsyncFileIO, public AdvancedService<SharedOverThreadsAsyncFileIO> {
    public:
        SharedOverThreadsAsyncFileIO(Properties props);

        Task<tl::expected<std::string, IOError>> readWholeFile(std::filesystem::path const &file) final;
        Task<tl::expected<void, IOError>> copyFile(std::filesystem::path const &from, std::filesystem::path const &to) final;
        Task<tl::expected<void, IOError>> removeFile(std::filesystem::path const &file) final;
        Task<tl::expected<void, IOError>> writeFile(std::filesystem::path const &file, std::string_view contents) final;
        Task<tl::expected<void, IOError>> appendFile(std::filesystem::path const &file, std::string_view contents) final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        static std::atomic<bool> _initialized;
        static std::mutex _io_mutex;
        static std::queue<std::shared_ptr<io_operation_submission>> _evts;
        std::atomic<bool> _should_stop{};
        tl::optional<std::thread> _io_thread;
    };
}
