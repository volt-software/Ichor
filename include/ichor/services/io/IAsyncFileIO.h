#pragma once

#include <ichor/coroutines/Task.h>
#include <string>
#include <string_view>
#include <filesystem>
#include <utility>
#include <tl/expected.h>
#include <fmt/core.h>

namespace Ichor {
    enum class FileIOError : uint_fast16_t {
        FAILED,
        FILE_DOES_NOT_EXIST,
        NO_PERMISSION,
        IS_DIR_SHOULD_BE_FILE,
        IS_FILE_SHOULD_BE_DIR,
        INTERRUPTED_BY_SIGNAL,
        BASENAME_CONTAINS_INVALID_CHARACTERS,
        TOO_MANY_SYMBOLIC_LINKS,
        PER_PROCESS_LIMIT_REACHED,
        FILEPATH_TOO_LONG,
        SERVICE_STOPPED,
        FILE_SIZE_TOO_BIG,
        SYSTEM_BUG,
        READ_ONLY_FS,
        USER_QUOTA_REACHED,
        NO_SPACE_LEFT
    };

    class IAsyncFileIO {
    public:

        /*
         * Reads the entire contents of a file into a std::string.
         */
        virtual Task<tl::expected<std::string, FileIOError>> readWholeFile(std::filesystem::path const &file) = 0;

        /*
         * Copies the contents of one file to another. This function will also copy the permission bits of the original file to the destination file.
         * This function will overwrite the contents of to.
         * Note that if from and to both point to the same file, then the file will likely get truncated by this operation.
         */
        virtual Task<tl::expected<void, FileIOError>> copyFile(std::filesystem::path const &from, std::filesystem::path const &to) = 0;

        /*
         * Removes a file from the filesystem.
         * Note that there is no guarantee that the file is immediately deleted (e.g., depending on platform, other open file descriptors may prevent immediate removal).
         */
        virtual Task<tl::expected<void, FileIOError>> removeFile(std::filesystem::path const &file) = 0;

        /*
         * Write a string as the entire contents of a file.
         * This function will create a file if it does not exist, and will entirely replace its contents if it does.
         * Depending on the platform, this function may fail if the full directory path does not exist.
         */
        virtual Task<tl::expected<void, FileIOError>> writeFile(std::filesystem::path const &file, std::string_view contents) = 0;
        /*
         * Append a string to a file.
         * This function will create a file if it does not exist, and will add the given contents regardless.
         * Depending on the platform, this function may fail if the full directory path does not exist.
         */
        virtual Task<tl::expected<void, FileIOError>> appendFile(std::filesystem::path const &file, std::string_view contents) = 0;

        /*
         * Function to make an errno indicator to an Ichor FileIOError. Returns FileIOError::FAILED if it doesn't recognize the number.
         */
        [[nodiscard]] static Ichor::FileIOError mapErrnoToError(std::remove_cvref_t<decltype(errno)> err) noexcept;

    protected:
        ~IAsyncFileIO() = default;
    };
}

//        FAILED,
//        FILE_DOES_NOT_EXIST,
//        NO_PERMISSION,
//        IS_DIR_SHOULD_BE_FILE,
//        INTERRUPTED_BY_SIGNAL,
//        BASENAME_CONTAINS_INVALID_CHARACTERS,
//        TOO_MANY_SYMBOLIC_LINKS,
//        PER_PROCESS_LIMIT_REACHED,
//        FILEPATH_TOO_LONG,
//        SERVICE_STOPPED,
//        FILE_SIZE_TOO_BIG

template <>
struct fmt::formatter<Ichor::FileIOError> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Ichor::FileIOError& change, FormatContext& ctx) {
        switch(change) {
            case Ichor::FileIOError::FAILED:
                return fmt::format_to(ctx.out(), "FAILED");
            case Ichor::FileIOError::FILE_DOES_NOT_EXIST:
                return fmt::format_to(ctx.out(), "FILE_DOES_NOT_EXIST");
            case Ichor::FileIOError::NO_PERMISSION:
                return fmt::format_to(ctx.out(), "NO_PERMISSION");
            case Ichor::FileIOError::IS_DIR_SHOULD_BE_FILE:
                return fmt::format_to(ctx.out(), "IS_DIR_SHOULD_BE_FILE");
            case Ichor::FileIOError::IS_FILE_SHOULD_BE_DIR:
                return fmt::format_to(ctx.out(), "IS_FILE_SHOULD_BE_DIR");
            case Ichor::FileIOError::INTERRUPTED_BY_SIGNAL:
                return fmt::format_to(ctx.out(), "INTERRUPTED_BY_SIGNAL");
            case Ichor::FileIOError::BASENAME_CONTAINS_INVALID_CHARACTERS:
                return fmt::format_to(ctx.out(), "BASENAME_CONTAINS_INVALID_CHARACTERS");
            case Ichor::FileIOError::TOO_MANY_SYMBOLIC_LINKS:
                return fmt::format_to(ctx.out(), "TOO_MANY_SYMBOLIC_LINKS");
            case Ichor::FileIOError::PER_PROCESS_LIMIT_REACHED:
                return fmt::format_to(ctx.out(), "PER_PROCESS_LIMIT_REACHED");
            case Ichor::FileIOError::FILEPATH_TOO_LONG:
                return fmt::format_to(ctx.out(), "FILEPATH_TOO_LONG");
            case Ichor::FileIOError::SERVICE_STOPPED:
                return fmt::format_to(ctx.out(), "SERVICE_STOPPED");
            case Ichor::FileIOError::FILE_SIZE_TOO_BIG:
                return fmt::format_to(ctx.out(), "FILE_SIZE_TOO_BIG");
            case Ichor::FileIOError::SYSTEM_BUG:
                return fmt::format_to(ctx.out(), "SYSTEM_BUG");
            case Ichor::FileIOError::READ_ONLY_FS:
                return fmt::format_to(ctx.out(), "READ_ONLY_FS");
            case Ichor::FileIOError::USER_QUOTA_REACHED:
                return fmt::format_to(ctx.out(), "USER_QUOTA_REACHED");
            case Ichor::FileIOError::NO_SPACE_LEFT:
                return fmt::format_to(ctx.out(), "NO_SPACE_LEFT");
        }
        return fmt::format_to(ctx.out(), "error, please file a bug in Ichor");
    }
};
