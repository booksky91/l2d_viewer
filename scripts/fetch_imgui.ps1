$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path third_party | Out-Null
if (!(Test-Path third_party/imgui)) {
  git clone https://github.com/ocornut/imgui third_party/imgui
} else {
  Write-Host "third_party/imgui already exists"
}
