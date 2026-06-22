#!/usr/bin/env bash
set -euo pipefail
mkdir -p third_party
if [ ! -d third_party/imgui ]; then
  git clone https://github.com/ocornut/imgui third_party/imgui
else
  echo "third_party/imgui already exists"
fi
