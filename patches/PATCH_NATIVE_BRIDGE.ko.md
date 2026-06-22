# Cubism Native 실제 연결 패치 가이드

이 GUI는 Live2D 헤더를 직접 include하지 않고 `INativeCubismBridge` 인터페이스만 호출합니다. 이유는 Cubism Core/Native SDK가 GitHub에 포함되지 않고, 사용자가 공식 SDK 약관에 동의하고 별도로 받아야 하기 때문입니다.

## 가장 빠른 연결 방식

1. 공식 `CubismNativeSamples`의 `Samples/OpenGL/Demo/proj.win.cmake`가 먼저 빌드되는지 확인합니다.
2. 이 프로젝트의 `src/`를 Demo 프로젝트에 추가합니다.
3. `patches/CubismNativeSamplesBridge.cpp.example`를 `CubismNativeSamplesBridge.cpp`로 복사합니다.
4. CMake에 `-DL2D_USE_MOCK_BACKEND=OFF -DL2D_ENABLE_CUBISM_NATIVE=ON`을 줍니다.
5. include path가 Demo의 `src`, Framework, Core를 볼 수 있게 맞춥니다.

## LAppModel에 추가하면 좋은 공개 메서드

공식 샘플의 `LAppModel`은 뷰어 데모용이라 배치 export에 필요한 일부 제어가 public으로 나와 있지 않습니다.

```cpp
void LAppModel::StopAllMotions()
{
    _motionManager->StopAllMotions();
}

void LAppModel::UpdateWithDelta(float deltaTimeSeconds)
{
    // 기존 Update()의 LAppPal::GetDeltaTime() 사용 부분을 deltaTimeSeconds로 바꾼 버전.
    // deterministic export에서 필요합니다.
}
```

루프 모션을 정확히 하려면 `CubismMotion` 로드 직후 `SetLoop(true)` 또는 모션 완료 콜백에서 재시작하는 처리가 필요합니다. SDK 버전에 따라 `ACubismMotion::SetLoop` 공개 여부가 다릅니다.

## MVP 한계

- 현재 `CubismNativeSamplesBridge.cpp.example`는 SDK 버전별 시그니처 차이를 감안한 예시입니다.
- 실사용하려면 사용하는 SDK 버전의 `LAppModel`, `LAppDelegate`, `CubismMatrix44` API에 맞춰 1회 조정해야 합니다.
- GUI/Export/FFmpeg 파이프라인은 SDK와 분리되어 있으므로 브릿지만 완성하면 그대로 재사용됩니다.
