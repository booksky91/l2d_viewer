param(
  [string]$SdkRoot = "D:/workspace/dev/CubismSdkForNative-5-r.5"
)

Write-Host "SDK Root: $SdkRoot"
Write-Host "\nCore libs:"
Get-ChildItem -Recurse "$SdkRoot/Core/lib" -Filter "*CubismCore*.lib" -ErrorAction SilentlyContinue | Select-Object FullName
Write-Host "\nCore DLLs:"
Get-ChildItem -Recurse "$SdkRoot/Core/dll" -Filter "*CubismCore*.dll" -ErrorAction SilentlyContinue | Select-Object FullName
