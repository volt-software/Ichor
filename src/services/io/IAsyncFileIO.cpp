#include <ichor/services/io/IAsyncFileIO.h>

Ichor::FileIOError Ichor::IAsyncFileIO::mapErrnoToError(std::remove_cvref_t<decltype(errno)> err) noexcept {
    if(err == EACCES || err == EFAULT || err == EPERM) {
        return FileIOError::NO_PERMISSION;
    } else if(err == EINTR) {
        return FileIOError::INTERRUPTED_BY_SIGNAL;
    } else if(err == EINVAL) {
        return FileIOError::BASENAME_CONTAINS_INVALID_CHARACTERS;
    } else if(err == ELOOP) {
        return FileIOError::TOO_MANY_SYMBOLIC_LINKS;
    } else if(err == EMFILE) {
        return FileIOError::PER_PROCESS_LIMIT_REACHED;
    } else if(err == ENAMETOOLONG) {
        return FileIOError::FILEPATH_TOO_LONG;
    } else if(err == ENOENT) {
        return FileIOError::FILE_DOES_NOT_EXIST;
    } else if(err == EISDIR) {
        return FileIOError::IS_DIR_SHOULD_BE_FILE;
    } else if(err == ENOTDIR) {
        return FileIOError::IS_FILE_SHOULD_BE_DIR;
    } else if(err == EROFS) {
        return FileIOError::READ_ONLY_FS;
    } else if(err == EDQUOT) {
        return FileIOError::USER_QUOTA_REACHED;
    } else if(err == ENOSPC) {
        return FileIOError::NO_SPACE_LEFT;
    } else {
        return FileIOError::FAILED;
    }
}
