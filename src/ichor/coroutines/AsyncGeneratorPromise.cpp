#include <ichor/DependencyManager.h>
#include <ichor/coroutines/AsyncGeneratorPromiseBase.h>

// 0 has a specific meaning.
uint64_t constinit thread_local Ichor::Detail::AsyncGeneratorPromiseBase::_idCounter = 1;
