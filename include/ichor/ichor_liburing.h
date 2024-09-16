#pragma once

// liburing uses different conventions than Ichor, ignore them to prevent being spammed by warnings
#if defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#    pragma GCC diagnostic ignored "-Wshadow"
#    pragma GCC diagnostic ignored "-Wconversion"
#    pragma GCC diagnostic ignored "-Wpedantic"
#    pragma GCC diagnostic ignored "-Wpointer-arith"
#    pragma GCC diagnostic ignored "-Wcast-align"
#endif
#include "liburing.h"
#if defined( __GNUC__ )
#    pragma GCC diagnostic pop
#endif
