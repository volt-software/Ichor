#pragma once

// Glaze uses different conventions than Ichor, ignore them to prevent being spammed by warnings
#if defined( __GNUC__ )
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#    pragma GCC diagnostic ignored "-Wshadow"
#    pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <glaze/glaze.hpp>
#if defined( __GNUC__ )
#    pragma GCC diagnostic pop
#endif
