#pragma once

#include <ichor/services/io/IAsyncFileIO.h>
#include <ichor/coroutines/AsyncManualResetEvent.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

namespace Ichor {
    struct io_operation_submission final {
        Ichor::AsyncManualResetEvent evt;
        tl::expected<void, Ichor::FileIOError> result;
        std::function<void(decltype(result)&)> fn;
    };

    class SharedOverThreadsAsyncFileIO final : public IAsyncFileIO, public AdvancedService<SharedOverThreadsAsyncFileIO> {
    public:
        SharedOverThreadsAsyncFileIO(Properties props);

        Task<tl::expected<std::string, FileIOError>> read_whole_file(std::filesystem::path const &file) final;
        Task<tl::expected<void, FileIOError>> copy_file(std::filesystem::path const &from, std::filesystem::path const &to) final;
        Task<tl::expected<void, FileIOError>> remove_file(std::filesystem::path const &file) final;
        Task<tl::expected<void, FileIOError>> write_file(std::filesystem::path const &file, std::string_view contents) final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        static std::atomic<bool> _initialized;
        static std::mutex _io_mutex;
        static std::queue<std::shared_ptr<io_operation_submission>> _evts;
        bool _should_stop{};
        std::optional<std::thread> _io_thread;
    };
}
