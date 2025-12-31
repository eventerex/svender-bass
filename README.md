# SvenderBass (VST3)

## Prereqs (Windows 10/11)
- Visual Studio 2022 Build Tools (MSVC)
- CMake
- Ninja
- Steinberg VST3 SDK cloned to: `C:\Users\bjorn\SDK\vst3sdk`
- Windows Developer Mode enabled (needed for symlink into the VST3 folder)

## Developer Command Prompt (x64)
Open a VS dev prompt with MSVC in PATH:

cmd /k ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64"

Verify:
- `cl` prints the MSVC banner.

## Configure (Ninja)
From the dev prompt:

rmdir /s /q C:\Users\bjorn\src\SvenderBass\build
cmake -S C:\Users\bjorn\src\SvenderBass -B C:\Users\bjorn\src\SvenderBass\build -G Ninja -DVST3_SDK_ROOT=C:\Users\bjorn\SDK\vst3sdk

## Build (Debug)
cmake --build C:\Users\bjorn\src\SvenderBass\build --config Debug

## Build (Release)
cmake --build C:\Users\bjorn\src\SvenderBass\build --config Release

## Installed plugin location (symlink created by build)
%LOCALAPPDATA%\Programs\Common\VST3\SvenderBass.vst3

The actual plugin binary is:
%LOCALAPPDATA%\Programs\Common\VST3\SvenderBass.vst3\Contents\x86_64-win\SvenderBass.vst3

## Notes
- If you get: "A required privilege is not held by the client" during build,
  enable Developer Mode in Windows 10:
  Settings → Update & Security → For developers → Developer mode.
- If you see MSVC warning D9025 about /Zi vs /ZI, CMakeLists.txt normalizes it.

