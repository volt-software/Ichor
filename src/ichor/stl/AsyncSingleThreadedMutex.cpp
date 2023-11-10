#include <ichor/stl/AsyncSingleThreadedMutex.h>

Ichor::AsyncSingleThreadedLockGuard::~AsyncSingleThreadedLockGuard() {
    if(_has_lock) {
        _m->unlock();
    }
}

void Ichor::AsyncSingleThreadedLockGuard::unlock() {
    if(_has_lock) {
        _has_lock = false;
        _m->unlock();
    }
}
