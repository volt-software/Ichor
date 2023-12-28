#pragma once

#include <ichor/coroutines/Task.h>
#include <string>
#include <string_view>
#include <filesystem>
#include <tl/expected.h>

namespace Ichor {
    enum class FileIOError : uint_fast16_t {
        FAILED,
        FILE_DOES_NOT_EXIST,
        NO_PERMISSION,
        IS_DIR_SHOULD_BE_FILE
    };

    class IAsyncFileIO {
    public:

        /*
         * Reads the entire contents of a file into a std::string.
         */
        virtual Task<tl::expected<std::string, FileIOError>> read_whole_file(std::filesystem::path const &file) = 0;

        /*
         * Copies the contents of one file to another. This function will also copy the permission bits of the original file to the destination file.
         * This function will overwrite the contents of to.
         * Note that if from and to both point to the same file, then the file will likely get truncated by this operation.
         */
        virtual Task<tl::expected<void, FileIOError>> copy_file(std::filesystem::path const &from, std::filesystem::path const &to) = 0;

        /*
         * Removes a file from the filesystem.
         * Note that there is no guarantee that the file is immediately deleted (e.g., depending on platform, other open file descriptors may prevent immediate removal).
         */
        virtual Task<tl::expected<void, FileIOError>> remove_file(std::filesystem::path const &file) = 0;

        /*
         * Write a string as the entire contents of a file.
         * This function will create a file if it does not exist, and will entirely replace its contents if it does.
         * Depending on the platform, this function may fail if the full directory path does not exist.
         */
        virtual Task<tl::expected<void, FileIOError>> write_file(std::filesystem::path const &file, std::string_view contents) = 0;

    protected:
        ~IAsyncFileIO() = default;
    };
}
