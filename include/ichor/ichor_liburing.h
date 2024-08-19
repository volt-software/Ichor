#pragma once

// liburing uses different conventions than Ichor, ignore them to prevent being spammed by warnings
#if defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#    pragma GCC diagnostic ignored "-Wshadow"
#    pragma GCC diagnostic ignored "-Wconversion"
#    pragma GCC diagnostic ignored "-Wpedantic"
#    pragma GCC diagnostic ignored "-Wgnu-pointer-arith"
#    pragma GCC diagnostic ignored "-Wgnu-anonymous-struct"
#    pragma GCC diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#    pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#    pragma GCC diagnostic ignored "-Wnested-anon-types"
#    pragma GCC diagnostic ignored "-Wzero-length-array"
#    pragma GCC diagnostic ignored "-Wcast-align"
#    pragma GCC diagnostic ignored "-Wc99-extensions"
#endif
#include "liburing.h"
#if defined( __GNUC__ )
#    pragma GCC diagnostic pop
#endif
