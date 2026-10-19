// Compiled into every test executable (test/CMakeLists.txt): what lets ctest run the tests in parallel, each test case being its own process.
#include <catch2/catch_user_config.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <filesystem>
#include <fmt/base.h>
#include <system_error>

#if !defined(_WIN32)
#include <unistd.h>
#else
#include <process.h>
#endif

// The tests assert from more than one thread (an event loop's and the test's): without it, Catch2's assertions race on its state (CMakeLists.txt)
#if !defined(CATCH_CONFIG_THREAD_SAFE_ASSERTIONS)
#error "Catch2 has to be configured with CATCH_CONFIG_THREAD_SAFE_ASSERTIONS"
#endif

namespace {
    /// Runs the tests in a directory of their own, so that tests running at the same time don't use each other's files (most use relative paths,
    /// e.g. AsyncFileIO.txt). Removed again afterwards.
    class OwnWorkingDirectory final : public Catch::EventListenerBase {
    public:
        using Catch::EventListenerBase::EventListenerBase;

        void testRunStarting(Catch::TestRunInfo const &) override {
            std::error_code ec;
            _original = std::filesystem::current_path(ec);
            if(ec) {
                return;
            }
#if !defined(_WIN32)
            auto const pid = static_cast<long>(::getpid());
#else
            auto const pid = static_cast<long>(::_getpid());
#endif
            _directory = _original / ("ichor-test-" + std::to_string(pid));
            std::filesystem::remove_all(_directory, ec);
            if(!std::filesystem::create_directory(_directory, ec) || ec) {
                fmt::println("couldn't create {}: {}", _directory.string(), ec.message());
                _directory.clear();
                return;
            }
            std::filesystem::current_path(_directory, ec);
        }

        void testRunEnded(Catch::TestRunStats const &) override {
            if(_directory.empty()) {
                return;
            }
            std::error_code ec;
            std::filesystem::current_path(_original, ec);
            std::filesystem::remove_all(_directory, ec);
        }

    private:
        std::filesystem::path _original;
        std::filesystem::path _directory;
    };
}

CATCH_REGISTER_LISTENER(OwnWorkingDirectory)
