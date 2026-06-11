cmake_minimum_required(VERSION 3.22)

project(XNULLX_Plugins VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Self-contained binaries: statically link the MSVC runtime on Windows
# (matches the Projucer "Use static runtime" / -MT setting). Ignored on macOS.
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ── Pull in JUCE (downloaded automatically; nothing to commit) ────────────────
# Pinned to the same JUCE version used on Windows (Projucer 8.0.13).
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ── The four plugins ──────────────────────────────────────────────────────────
add_subdirectory(GrainBrain)
add_subdirectory(O2)
add_subdirectory(Bunka)
add_subdirectory(Foldspace)
