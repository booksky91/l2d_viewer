param(
  [string]$SdkRoot = "D:/workspace/dev/CubismSdkForNative-5-r.5",
  [string]$VcpkgRoot = "D:/workspace/dev/vcpkg",
  [string]$ImguiDir = "D:/workspace/dev/live2d_native_gui_viewer_mvp/third_party/imgui",
  [string]$CubismCoreLib = "",
  [string]$CubismCoreDll = ""
)

$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $PSScriptRoot)
Remove-Item -Recurse -Force .\build-native -ErrorAction SilentlyContinue

$args = @(
  "-S", ".",
  "-B", "build-native",
  "-A", "x64",
  "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake",
  "-DIMGUI_DIR=$ImguiDir",
  "-DCUBISM_SDK_ROOT=$SdkRoot",
  "-DL2D_USE_MOCK_BACKEND=OFF",
  "-DL2D_ENABLE_CUBISM_NATIVE=ON"
)
if ($CubismCoreLib -ne "") { $args += "-DCUBISM_CORE_LIB=$CubismCoreLib" }
if ($CubismCoreDll -ne "") { $args += "-DCUBISM_CORE_DLL=$CubismCoreDll" }

cmake @args
if ($LASTEXITCODE -ne 0) {
  throw "CMake configure failed. Build step skipped."
}

cmake --build build-native --config Release
if ($LASTEXITCODE -ne 0) {
  throw "CMake build failed."
}
