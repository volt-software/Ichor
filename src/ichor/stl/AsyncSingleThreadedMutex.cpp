#include <ichor/stl/AsyncSingleThreadedMutex.h>

Ichor::v1::AsyncSingleThreadedLockGuard::~AsyncSingleThreadedLockGuard() {
    if(_has_lock) {
        _m->unlock();
    }
}

void Ichor::v1::AsyncSingleThreadedLockGuard::unlock() {
    if(_has_lock) {
        _has_lock = false;
        _m->unlock();
    }
}
