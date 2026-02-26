cmake_minimum_required(VERSION 3.20)
project(PacketGod C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
#  Sanity check — must be 32-bit (wow.exe is x86)
# ============================================================
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "PacketGod must be compiled as 32-bit (x86). "
            "Use -A Win32 or set CMAKE_GENERATOR_PLATFORM=Win32")
endif()

# ============================================================
#  Vendor paths  — adjust if you put libs elsewhere
# ============================================================
set(MINHOOK_DIR  "${CMAKE_SOURCE_DIR}/vendor/minhook")
set(IMGUI_DIR    "${CMAKE_SOURCE_DIR}/vendor/imgui")
set(DX9SDK_DIR   "$ENV{DXSDK_DIR}" CACHE PATH "DirectX SDK root (for d3d9.h)")

# ============================================================
#  MinHook  (static lib, pre-built or build from source)
# ============================================================
add_library(minhook STATIC
    "${MINHOOK_DIR}/src/buffer.c"
    "${MINHOOK_DIR}/src/hook.c"
    "${MINHOOK_DIR}/src/trampoline.c"
    "${MINHOOK_DIR}/src/hde/hde32.c"
)
target_include_directories(minhook PUBLIC "${MINHOOK_DIR}/include")
set_property(TARGET minhook PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ============================================================
#  ImGui  (Dear ImGui + DX9 + Win32 backends)
# ============================================================
add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx9.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
)
target_include_directories(imgui PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)
if(DX9SDK_DIR)
    target_include_directories(imgui PUBLIC "${DX9SDK_DIR}/Include")
endif()
target_link_libraries(imgui PUBLIC d3d9)
set_property(TARGET imgui PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ============================================================
#  PacketGod DLL
# ============================================================
add_library(PacketGod SHARED
    src/dllmain.cpp
    src/DebugLog.cpp

    src/hooks/HookManager.cpp
    src/hooks/PacketHooks.cpp
    src/hooks/D3DHooks.cpp

    src/packet/PacketCapture.cpp
    src/packet/PacketReplay.cpp

    src/ui/PacketUI.cpp
)

target_include_directories(PacketGod PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}"     # for Opcodes.h
)

target_link_libraries(PacketGod PRIVATE
    minhook
    imgui
    d3d9
)

# Optional: disable AuthChallenge hook if login crashes at handshake (0x00467C0B).
# Usage: cmake .. -DPACKETGOD_DISABLE_AUTH_CHALLENGE_HOOK=ON
option(PACKETGOD_DISABLE_AUTH_CHALLENGE_HOOK "Disable NetClient::AuthChallenge hook (workaround for login AV)" OFF)

# Suppress CRT-security warnings, enable optimizations in release
target_compile_definitions(PacketGod PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    $<$<BOOL:${PACKETGOD_DISABLE_AUTH_CHALLENGE_HOOK}>:PACKETGOD_DISABLE_AUTH_CHALLENGE_HOOK=1>
)

# Match WoW's MSVC runtime (MT for release, MTd for debug)
set_property(TARGET PacketGod PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Name the output PacketGod.dll
set_target_properties(PacketGod PROPERTIES
    OUTPUT_NAME "PacketGod"
    SUFFIX      ".dll"
)
