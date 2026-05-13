# WinUHid Unit Tests

## Steam Controller Tests

`SteamController.cpp` covers the WinUHidDevs 2026 Steam Controller / Triton BLE
preset.

The pure tests validate report layout, neutral report initializers, invalid
report IDs, and invalid raw input report sizes. The driver-backed tests create a
real WinUHid VHF child and validate that:

- the Steam Controller HID child enumerates;
- raw Triton input reports can be read back through HIDAPI;
- output and feature reports are forwarded to the raw HID callback unchanged,
  including the report ID byte.

The driver-backed tests intentionally use the preset Valve/Triton VID/PID
`28de:1303` and only override the instance ID. This matches how Apollo creates
the virtual Steam Controller. The test HID open path filters out `BTHLEDevice`
paths so a directly-paired physical Bluetooth Steam Controller is not used by
the write/feature tests.

## Build

From the workspace root:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  .\WinUHid\WinUHidUnitTests\WinUHidUnitTests.vcxproj `
  /p:Configuration=Debug `
  /p:Platform=x64 `
  /p:VcpkgManifestInstall=false `
  /p:VcpkgInstalledDir=C:\Users\chenh\Documents\steam_controller_study\WinUHid\WinUHidUnitTests\vcpkg_installed\ `
  /m
```

The explicit `VcpkgManifestInstall=false` and `VcpkgInstalledDir=...` arguments
avoid re-running manifest installation during normal incremental test builds.

## Run

The installed WinUHid driver currently exposes `\\.\WinUHid` with an ACL that
allows Builtin Administrators, LocalSystem, and UMDF drivers. A normal
non-elevated UAC token opens it as `ACCESS_DENIED`, which is expected behavior
with the current INF security descriptor.

Use the wrapper from the repo:

```powershell
.\WinUHid\WinUHidUnitTests\run_steam_controller_tests_elevated.ps1
```

The wrapper re-launches itself through UAC, runs
`WinUHidUnitTests.exe --gtest_filter=SteamController.*`, and writes the log to:

```text
WinUHid\WinUHidUnitTests\build\test_logs\
```

Verified on 2026-05-10 with Debug x64: all seven `SteamController.*` tests
passed when run elevated against the installed WinUHid driver.
