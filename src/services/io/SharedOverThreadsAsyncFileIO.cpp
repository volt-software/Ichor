#include <atomic>
#include <functional>
#include <chrono>
#include <fmt/core.h>
#include <fstream>
#include <ichor/services/io/SharedOverThreadsAsyncFileIO.h>
#include <ichor/event_queues/IEventQueue.h>
#include <ichor/events/RunFunctionEvent.h>
#include <fmt/os.h>
#include <ichor/ScopeGuard.h>
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
#include <winbase.h>
#include <errhandlingapi.h>
#elif defined(__APPLE__)
#include <copyfile.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

using namespace std::chrono_literals;

std::atomic<bool> Ichor::SharedOverThreadsAsyncFileIO::_initialized;
std::mutex Ichor::SharedOverThreadsAsyncFileIO::_io_mutex;
std::queue<std::shared_ptr<Ichor::io_operation_submission>> Ichor::SharedOverThreadsAsyncFileIO::_evts;

Ichor::SharedOverThreadsAsyncFileIO::SharedOverThreadsAsyncFileIO(Ichor::Properties props) : AdvancedService<SharedOverThreadsAsyncFileIO>(std::move(props)) {

}

Ichor::Task<tl::expected<void, Ichor::StartError>> Ichor::SharedOverThreadsAsyncFileIO::start() {
    INTERNAL_IO_DEBUG("setup_thread");
    if (!_initialized.exchange(true, std::memory_order_acq_rel)) {
        auto &queue = Ichor::GetThreadLocalEventQueue();

        _io_thread = std::thread([this, &queue = queue]() {
            INTERNAL_IO_DEBUG("io_thread");
            while(!_should_stop) {
                INTERNAL_IO_DEBUG("checking");
                {
                    std::unique_lock lg{_io_mutex};
                    while (!_evts.empty()) {
                        INTERNAL_IO_DEBUG("processing submission");
                        auto submission = std::move(_evts.front());
                        _evts.pop();
                        lg.unlock();
                        submission->fn(submission->result);
                        queue.pushEvent<Ichor::RunFunctionEvent>(0, [submission = std::move(submission)]() mutable {
                            submission->evt.set();
                        });
                    }
                }
                std::this_thread::sleep_for(10ms);
                INTERNAL_IO_DEBUG("checking post-sleep");
            }
            INTERNAL_IO_DEBUG("io_thread done");
        });
    }
    co_return {};
}

Ichor::Task<void> Ichor::SharedOverThreadsAsyncFileIO::stop() {
    if(_io_thread && _io_thread->joinable()) {
        _should_stop = true;
        _io_thread->join();
        _initialized.store(false, std::memory_order_release);
    }
    co_return;
}

Ichor::Task<tl::expected<std::string, Ichor::FileIOError>> Ichor::SharedOverThreadsAsyncFileIO::read_whole_file(std::filesystem::path const &file_path) {
    INTERNAL_IO_DEBUG("read_whole_file()");

    auto submission = std::make_shared<io_operation_submission>();
    std::string contents;
    submission->fn = [&file_path, &contents](decltype(io_operation_submission::result) &res) {
        INTERNAL_IO_DEBUG("submission->fn()");
        std::ifstream file(file_path);

        if(!file) {
            if(!std::filesystem::exists(file_path)) {
                res = tl::unexpected(FileIOError::FILE_DOES_NOT_EXIST);
                return;
            }
            INTERNAL_IO_DEBUG("!file");
            res = tl::unexpected(FileIOError::FAILED);
            return;
        }

        file.seekg(0, std::ios::end);
        contents.resize(static_cast<unsigned long>(file.tellg()));
        file.seekg(0, std::ios::beg);
        file.read(contents.data(), static_cast<long>(contents.size()));
        file.close();

        INTERNAL_IO_DEBUG("submission->fn() contents: {}", contents);
    };
    {
        std::unique_lock lg{_io_mutex};
        _evts.push(submission);
    }
    INTERNAL_IO_DEBUG("co_await");
    co_await submission->evt;

    if(!submission->result) {
        INTERNAL_IO_DEBUG("!result");
        co_return tl::unexpected(submission->result.error());
    }
    INTERNAL_IO_DEBUG("contents: {}", contents);

    co_return contents;
}

Ichor::Task<tl::expected<void, Ichor::FileIOError>> Ichor::SharedOverThreadsAsyncFileIO::copy_file(const std::filesystem::path &from, const std::filesystem::path &to) {
    INTERNAL_IO_DEBUG("copy_file()");

    auto submission = std::make_shared<io_operation_submission>();
    submission->fn = [&from, &to](decltype(io_operation_submission::result) &res) {
        INTERNAL_IO_DEBUG("submission->fn()");
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
        BOOL success = CopyFileExA(from.string().c_str(), to.string().c_str(), nullptr, nullptr, nullptr, 0);
        if(success == 0) {
            DWORD err = GetLastError();
            INTERNAL_IO_DEBUG("CopyFileExA failed: {}", err);
            if(err == ERROR_FILE_NOT_FOUND) {
                res = tl::unexpected(FileIOError::FILE_DOES_NOT_EXIST);
                return;
            }

            INTERNAL_IO_DEBUG("CopyFileExA failed: {}", GetLastError());
            res = tl::unexpected(FileIOError::FAILED);
            return;
        }
#elif defined(__APPLE__)
        auto state = copyfile_state_alloc();
        ScopeGuard sg_state([&state]() {
            copyfile_state_free(state);
        });
        auto ret = copyfile(from.string().c_str(), to.string().c_str(), state, COPYFILE_ALL);
        auto err = errno;
        if(ret < 0) {
            INTERNAL_IO_DEBUG("copyfile ret: {}, errno: {}", ret, err);
            if(err == EPERM) {
                res = tl::unexpected(FileIOError::NO_PERMISSION);
                return;
            }
            if(err == EISDIR) {
                res = tl::unexpected(FileIOError::IS_DIR_SHOULD_BE_FILE);
                return;
            }
            if(err == ENOENT) {
                res = tl::unexpected(FileIOError::FILE_DOES_NOT_EXIST);
                return;
            }
            res = tl::unexpected(FileIOError::FAILED);
            return;
        }
#else
        struct stat stat;
        off_t len, ret;
        int fd_in = open(from.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd_in == -1) {
            INTERNAL_IO_DEBUG("open from errno {}", errno);
            res = tl::unexpected(FileIOError::FILE_DOES_NOT_EXIST);
            return;
        }
        ScopeGuard sg_fd_in{[fd_in]() {
            close(fd_in);
        }};

        if (fstat(fd_in, &stat) == -1) {
            INTERNAL_IO_DEBUG("fstat from errno {}", errno);
            res = tl::unexpected(FileIOError::FILE_DOES_NOT_EXIST);
            return;
        }

        len = stat.st_size;

        int fd_out = open(to.c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0644);
        if (fd_out == -1) {
            INTERNAL_IO_DEBUG("open to errno {}", errno);
            res = tl::unexpected(FileIOError::FAILED);
            return;
        }
        ScopeGuard sg_fd_out{[fd_out]() {
            close(fd_out);
        }};

        do {
            ret = copy_file_range(fd_in, nullptr, fd_out, nullptr, static_cast<size_t>(len), 0);
            if (ret == -1) {
                int err = errno;
                INTERNAL_IO_DEBUG("copy_file_range failed: {}", err);
                if(err == EPERM) {
                    res = tl::unexpected(FileIOError::NO_PERMISSION);
                    return;
                }
                if(err == EISDIR) {
                    res = tl::unexpected(FileIOError::IS_DIR_SHOULD_BE_FILE);
                    return;
                }
                res = tl::unexpected(FileIOError::FAILED);
                return;
            }

            len -= ret;
        } while (len > 0 && ret > 0);

        INTERNAL_IO_DEBUG("submission->fn() copied bytes: {}", stat.st_size);
#endif
    };
    {
        std::unique_lock lg{_io_mutex};
        _evts.push(submission);
    }
    INTERNAL_IO_DEBUG("co_await");
    co_await submission->evt;

    if(!submission->result) {
        INTERNAL_IO_DEBUG("!result");
        co_return tl::unexpected(submission->result.error());
    }

    co_return {};
}

Ichor::Task<tl::expected<void, Ichor::FileIOError>> Ichor::SharedOverThreadsAsyncFileIO::remove_file(const std::filesystem::path &file) {
    INTERNAL_IO_DEBUG("remove_file()");

    auto submission = std::make_shared<io_operation_submission>();
    submission->fn = [&file](decltype(io_operation_submission::result) &res) {
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32)) && !defined(__CYGWIN__)
        BOOL ret = DeleteFile(file.string().c_str());

        if(ret == 0) {
            DWORD err = GetLastError();
            INTERNAL_IO_DEBUG("DeleteFile failed: {}", err);
            if(err == ERROR_FILE_NOT_FOUND) {
                res = tl::unexpected(FileIOError::FILE_DOES_NOT_EXIST);
                return;
            }
            if(err == ERROR_ACCESS_DENIED) {
                res = tl::unexpected(FileIOError::NO_PERMISSION);
                return;
            }
            res = tl::unexpected(FileIOError::FAILED);
        }
#else
        int ret = unlink(file.string().c_str());

        if(ret != 0) {
            int err = errno;
            INTERNAL_IO_DEBUG("unlink failed: {}", err);
            if(err == EACCES || err == EPERM) {
                res = tl::unexpected(FileIOError::NO_PERMISSION);
                return;
            }
            if(err == ENOENT) {
                res = tl::unexpected(FileIOError::FILE_DOES_NOT_EXIST);
                return;
            }
            if(err == EISDIR) {
                res = tl::unexpected(FileIOError::IS_DIR_SHOULD_BE_FILE);
                return;
            }
            res = tl::unexpected(FileIOError::FAILED);
        }
#endif
    };
    {
        std::unique_lock lg{_io_mutex};
        _evts.push(submission);
    }
    INTERNAL_IO_DEBUG("co_await");
    co_await submission->evt;

    if(!submission->result) {
        INTERNAL_IO_DEBUG("!result");
        co_return tl::unexpected(submission->result.error());
    }

    co_return {};
}

Ichor::Task<tl::expected<void, Ichor::FileIOError>> Ichor::SharedOverThreadsAsyncFileIO::write_file(const std::filesystem::path &file, std::string_view contents) {

    INTERNAL_IO_DEBUG("write_file()");

    auto submission = std::make_shared<io_operation_submission>();
    submission->fn = [&file, contents](decltype(io_operation_submission::result) &res) {
        try {
            auto out = fmt::output_file(file.string());
            out.print("{}", contents);
        } catch (const std::system_error &e) {
            INTERNAL_IO_DEBUG("write_file failed: {} {}", e.code().value(), e.code().message());
            int err = e.code().value();
            if(err == EACCES || err == EPERM) {
                res = tl::unexpected(FileIOError::NO_PERMISSION);
                return;
            }
            if(err == ENOENT) {
                res = tl::unexpected(FileIOError::FILE_DOES_NOT_EXIST);
                return;
            }
            if(err == EISDIR) {
                res = tl::unexpected(FileIOError::IS_DIR_SHOULD_BE_FILE);
                return;
            }
            res = tl::unexpected(FileIOError::FAILED);
        }
    };
    {
        std::unique_lock lg{_io_mutex};
        _evts.push(submission);
    }
    INTERNAL_IO_DEBUG("co_await");
    co_await submission->evt;

    if(!submission->result) {
        INTERNAL_IO_DEBUG("!result");
        co_return tl::unexpected(submission->result.error());
    }

    co_return {};
}
