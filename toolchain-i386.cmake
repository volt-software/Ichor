# toolchain-i386.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86)       # hint only

# Force 32-bit codegen
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -m32" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32" CACHE STRING "" FORCE)

# Tell CMake where 32-bit libs live
list(APPEND CMAKE_LIBRARY_PATH /usr/lib/i386-linux-gnu)
list(APPEND CMAKE_SYSTEM_LIBRARY_PATH /usr/lib/i386-linux-gnu)

# Make pkg-config resolve to i386 .pc files
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_PATH}  "")  # avoid accidental amd64 paths

set(OPENSSL_USE_STATIC_LIBS ON)
