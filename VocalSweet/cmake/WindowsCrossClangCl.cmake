# Cross-compile toolchain: Linux host -> Windows x64 (MSVC ABI) using clang-cl + lld-link
# and the MSVC CRT / Windows SDK headers+libs obtained with `xwin splat`.
#
# Usage:
#   cmake -B build-win -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/WindowsCrossClangCl.cmake \
#         -DXWIN_DIR=/opt/xwin-sdk -DCMAKE_BUILD_TYPE=Release
#
# NOTE: JUCE does not officially support cross-compiling for Windows. This toolchain works
# for VocalSweet because the output is a plain MSVC-ABI DLL; validate the binary on Windows.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT XWIN_DIR)
    set(XWIN_DIR "$ENV{XWIN_DIR}")
endif()
if(NOT XWIN_DIR)
    set(XWIN_DIR "/opt/xwin-sdk")
endif()
# Make XWIN_DIR visible inside try_compile projects
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES XWIN_DIR)

# The MSVC STL from current Visual Studio requires clang >= 19; prefer a versioned LLVM install.
set(_llvm_hints /usr/lib/llvm-20/bin /usr/lib/llvm-19/bin)
find_program(CLANG_CL_EXE NAMES clang-cl HINTS ${_llvm_hints} REQUIRED)
find_program(LLD_LINK_EXE NAMES lld-link HINTS ${_llvm_hints} REQUIRED)
find_program(LLVM_RC_EXE  NAMES llvm-rc  HINTS ${_llvm_hints} REQUIRED)
find_program(LLVM_LIB_EXE NAMES llvm-lib HINTS ${_llvm_hints} REQUIRED)
find_program(LLVM_MT_EXE  NAMES llvm-mt  HINTS ${_llvm_hints})

set(CMAKE_C_COMPILER   "${CLANG_CL_EXE}")
set(CMAKE_CXX_COMPILER "${CLANG_CL_EXE}")
set(CMAKE_LINKER       "${LLD_LINK_EXE}")
set(CMAKE_AR           "${LLVM_LIB_EXE}")
set(CMAKE_RC_COMPILER  "${LLVM_RC_EXE}")
if(LLVM_MT_EXE)
    set(CMAKE_MT "${LLVM_MT_EXE}")
endif()

set(CMAKE_C_COMPILER_TARGET   x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

set(_xwin_inc
    "/imsvc${XWIN_DIR}/crt/include"
    "/imsvc${XWIN_DIR}/sdk/include/ucrt"
    "/imsvc${XWIN_DIR}/sdk/include/um"
    "/imsvc${XWIN_DIR}/sdk/include/shared"
    "/imsvc${XWIN_DIR}/sdk/include/winrt")
string(JOIN " " _xwin_inc_str ${_xwin_inc})

# -Wno-* : JUCE + Windows SDK headers are noisy under clang-cl
set(_common "--target=x86_64-pc-windows-msvc -fuse-ld=lld-link ${_xwin_inc_str} -Wno-unused-command-line-argument -Wno-nonportable-include-path -Wno-pragma-pack -Wno-ignored-pragma-intrinsic -Wno-microsoft-anon-tag -Wno-unknown-pragmas")
set(CMAKE_C_FLAGS_INIT   "${_common}")
set(CMAKE_RC_FLAGS_INIT  "/I ${XWIN_DIR}/sdk/include/um /I ${XWIN_DIR}/sdk/include/shared /I ${XWIN_DIR}/crt/include /I ${XWIN_DIR}/sdk/include/ucrt")
set(CMAKE_CXX_FLAGS_INIT "${_common} /EHsc")

set(_xwin_lib
    "/libpath:${XWIN_DIR}/crt/lib/x86_64"
    "/libpath:${XWIN_DIR}/sdk/lib/um/x86_64"
    "/libpath:${XWIN_DIR}/sdk/lib/ucrt/x86_64")
string(JOIN " " _xwin_lib_str ${_xwin_lib})
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_xwin_lib_str}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_xwin_lib_str}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_xwin_lib_str}")

# Static CRT: the plugin will not require the Visual C++ redistributable on the user's PC.
# xwin ships only release CRT libs, so always use the release static CRT (also for try_compile).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
