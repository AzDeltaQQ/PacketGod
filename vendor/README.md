# Vendor Dependencies

Place third-party libraries here before building.

## MinHook

```
vendor/minhook/
├── include/
│   └── MinHook.h
└── src/
    ├── buffer.c
    ├── hook.c
    ├── trampoline.c
    └── hde/
        ├── hde32.c
        └── hde32.h
```

**Download**: https://github.com/TsudaKageyu/minhook
`git clone https://github.com/TsudaKageyu/minhook vendor/minhook`

## Dear ImGui

```
vendor/imgui/
├── imgui.h
├── imgui.cpp
├── imgui_draw.cpp
├── imgui_tables.cpp
├── imgui_widgets.cpp
└── backends/
    ├── imgui_impl_dx9.h
    ├── imgui_impl_dx9.cpp
    ├── imgui_impl_win32.h
    └── imgui_impl_win32.cpp
```

**Download**: https://github.com/ocornut/imgui
`git clone https://github.com/ocornut/imgui vendor/imgui`

## DirectX SDK

Required for `d3d9.h`.
Install the **June 2010 DirectX SDK** from Microsoft.
Set the `DXSDK_DIR` environment variable (usually set automatically by the installer).

Alternatively, use the `directx-headers` NuGet package or the Windows SDK (Win8+).
