# Live2D GUI Viewer v2.0 (Native MVP)

C++ / OpenGL / Dear ImGui 기반의 **Live2D 다중 모델 실시간 배치 뷰어 및 투명 비디오/이미지 시퀀스 고품질 내보내기(Export) 도구**입니다.  
Cubism Editor 결제 없이도 Native SDK 런타임의 그래픽 콘텍스트를 오프스크린 FBO(Frame Buffer Object)로 가로채고, FFmpeg 파이프라인과 연동하여 **알파 투명 채널이 보존된 WebM(VP9) 영상 및 디더링 최적화된 GIF** 등을 초고화질로 인코딩하여 추출합니다.

---

## 🌟 프로그램 핵심 기능 (Core Features)

### 1. 다중 모델 실시간 드래그 배치 & 편집
- **간편한 드래그 앤 드롭**: `.model3.json` 파일 또는 모델이 포함된 폴더를 창에 드래그하여 즉시 가져옵니다.
- **개별 모델 트랜스폼**: 화면에 로드된 여러 개의 모델을 독립적으로 선택하고, 불투명도(Alpha), 스케일(Scale), 회전(Rotation), X/Y 위치를 실시간 슬라이더 또는 마우스 조작으로 조정하여 자유롭게 화면 레이아웃을 구성합니다.
- **모델 레이어 순서 재정렬**: 모델의 드로잉 우선순위 레이어를 마우스 드래그로 손쉽게 정렬할 수 있습니다.

### 2. 모션 시퀀스 플레이리스트 (Playlist) 재생
- **연속 모션 연출**: 복수의 모션 슬롯을 생성하여 원하는 재생 순서대로 순차 재생할 수 있습니다.
- **정밀 제어**: 개별 모션 간 전환을 부드럽게 이어주는 페이드인 타임(Fade-in Time) 설정, 모션 반복(Loop), 자동 대기(Auto Idle) 토글 기능이 탑재되어 있습니다.
- **모션 리셋 잔상 제거**: 루프가 처음으로 돌아가거나 사용자가 시퀀스를 편집하여 리스타트할 때, 페이드 아웃 블렌딩 잔상을 완벽히 제거(`StopAllMotions` 강제 정지 연동)하여 깨끗하게 연출합니다.

### 3. 동적 마우스 응시 트래킹 (Look-Target Tracking)
- 뷰포트 영역에서 마우스 우클릭 드래그 시, 모델의 시선과 고개가 마우스 위치를 자연스럽게 추적(Gaze Tracking)하도록 처리하여 라이브 모델 인터랙션을 구성할 수 있습니다.

### 4. 세부 파라미터 및 파트 투명도 제어
- 모델 고유의 파라미터(Parameter) 값 슬라이더 조정 및 기본값 복원 기능 제공.
- 파트(Part)별 투명도 및 표시 여부를 동적으로 켜고 꺼서 커스터마이징된 모델 조작 상태를 만듭니다.

### 5. 초고화질/투명 알파 채널 내보내기 (Export)
- **투명 채널 보존 영상**: VP9 코덱을 탑재한 투명 WebM 비디오, APNG, WebP 애니메이션 파일 출력을 완벽히 지원합니다.
- **이미지 시퀀스**: 투명도가 포함된 개별 프레임 이미지(PNG, WebP, JPG) 시퀀스 추출이 가능합니다.
- **자동 감지 해상도 (Auto-bounding)**: 첫 프레임의 알파 채널 불투명 범위를 탐색하여 모델에 꼭 맞는 최소 크롭 상자(Bounding Box) 크기를 측정하고 자동 크롭 인코딩을 수행합니다.
- **Supersampling (Render Scale)**: 가시 뷰포트보다 큰 2x, 4x 고해상도 버퍼에 먼저 그린 뒤, FFmpeg의 Lanczos 축소 필터로 리사이징하여 계단 현상이 완전히 억제된 극상의 화질을 만듭니다.
- **여백 마진(Margin) 패딩**: 사방에 원하는 크기의 투명 여백 마진을 안정적으로 삽입합니다.

### 6. 작업 세션 백업 및 실행 제어 (v2.0 업데이트)
- **프로젝트 직렬화(`.l2dproj`)**: 전체 화면 배치 상태, 카메라 줌/팬, 플레이리스트 재생 위치, 활성 표정 상태까지 일괄 JSON으로 백업하고 원클릭 복원합니다.
- **50단계 Undo/Redo**: 모델 로드/삭제, 트랜스폼 조작, 플레이리스트 편집 시점을 자동으로 스냅샷 기록하여 자유로운 실행 취소 및 다시 실행을 제공합니다. 슬라이더 조작이 멈추는 순간에만 히스토리를 저장해 메모리 팽창을 방지합니다.
- **배경 제어 및 체커보드**: 투명 배경 작업 시 격자무늬 체커보드를 띄우고, 로컬 이미지 배경 투사 시 화면 맞춤 방식(Fill / Fit / Stretch)을 유연하게 제어합니다.
- **Export Preset**: 출력 포맷 설정을 파일로 만들어 언제든 다시 불러올 수 있는 프리셋 콤보박스 시스템이 통합되었습니다.

---

## 📁 폴더 구조

- `src/` : 애플리케이션의 핵심 C++ 소스 코드 (GUI, 렌더러, 직렬화, 내보내기 로직 등)
- `cmake/` : CMake 빌드 도구 및 모듈 파일
- `patches/` : Live2D Cubism Native SDK와 링킹하기 위한 브릿지 샘플 패치 파일
- `scripts/` : 서브 모듈이나 ImGui 라이브러리를 가져오는 자동화 스크립트 (예: `fetch_imgui.ps1`)
- `CMakeLists.txt` : 전체 빌드를 정의한 프로젝트 구성 파일
- `*.md` : 프로젝트 소개 및 설계, 패치 안내 등 마크다운 문서 (`README.md`, `DESIGN.ko.md` 등)

---

## 🛠️ 개발 및 빌드 가이드

### 1. 필수 의존성 준비 (Windows 환경 권장)

이 프로젝트는 패키지 매니저로 `vcpkg`를 사용합니다. 빌드 전에 아래 라이브러리들을 설치해야 합니다.

```powershell
# vcpkg를 이용한 필수 종속성 설치
vcpkg install glfw3:x64-windows glew:x64-windows stb:x64-windows nlohmann-json:x64-windows

# Dear ImGui 소스 코드 가져오기 (scripts 폴더 내부 스크립트 실행)
./scripts/fetch_imgui.ps1
```

### 2. CMake 빌드 및 실행

#### A. Dummy Mock Backend로 빌드 (Live2D SDK 없이 GUI만 테스트)
Live2D 공식 SDK 라이선스가 없더라도 UI와 내보내기 파이프라인의 레이아웃 흐름을 즉시 테스트해볼 수 있는 더미 모드입니다.

```powershell
# 빌드 구성 및 생성
cmake -S . -B build-native `
  -DCMAKE_TOOLCHAIN_FILE=[vcpkg_설치_경로]/scripts/buildsystems/vcpkg.cmake `
  -DIMGUI_DIR=$PWD/third_party/imgui `
  -DL2D_USE_MOCK_BACKEND=ON

# 컴파일
cmake --build build-native --config Release

# 실행
./build-native/Release/l2d_viewer_gui.exe
```

#### B. Live2D Cubism Native SDK 연동 빌드 (실제 렌더링 활성화)
실제 `.model3.json` 파일을 가져와서 화면에 출력하려면 다음을 구성합니다.

```powershell
cmake -S . -B build-native `
  -DCMAKE_TOOLCHAIN_FILE=[vcpkg_설치_경로]/scripts/buildsystems/vcpkg.cmake `
  -DIMGUI_DIR=$PWD/third_party/imgui `
  -DL2D_USE_MOCK_BACKEND=OFF `
  -DL2D_ENABLE_CUBISM_NATIVE=ON `
  -DCUBISM_SDK_ROOT=[Live2D_Cubism_Native_SDK_Root_경로]
```
> ⚠️ **중요**: 실제 Native 연동을 완료하기 전에 `patches/CubismNativeSamplesBridge.cpp.example`을 참고해 SDK 샘플 내 렌더 스레드 브릿지를 먼저 배치해야 합니다. 자세한 구현 가이드는 [patches/PATCH_NATIVE_BRIDGE.ko.md](file:///d:/workspace/dev/live2d_native_gui_viewer_mvp/patches/PATCH_NATIVE_BRIDGE.ko.md) 문서를 확인해 주세요.


---

## ⌨️ 기본 단축키
- `Ctrl + O` : 프로젝트 파일(`.l2dproj`) 불러오기
- `Ctrl + S` : 프로젝트 빠른 저장
- `Ctrl + Shift + S` : 다른 이름으로 프로젝트 저장 (저장 경로 다이얼로그 팝업)
- `Ctrl + Z` : 이전 작업 실행 취소 (Undo)
- `Ctrl + Y` 또는 `Ctrl + Shift + Z` : 실행 취소된 작업 다시 실행 (Redo)
- `Delete` : 활성 포커스 리스트(모델 리스트 또는 모션 플레이리스트)의 선택 항목 삭제

---

## ⚠️ 라이선스 및 저작권 고지 (Licensing Notice)

- **본 프로그램 코드 (Main Application Code)**:
  - Copyright (c) 2026.
  - 본 프로그램의 소스 코드는 **GNU General Public License v3.0 (GPL-3.0)** 라이선스 하에 배포되는 오픈소스 소프트웨어입니다.
  - 사용자는 자유롭게 코드를 활용, 수정, 재배포 및 상업적 사용을 할 수 있지만, 이 코드를 활용하여 제작된 2차 저작물 또한 동일한 GPL v3 라이선스로 투명하게 소스 코드를 공개해야 합니다. 상세 조항은 [LICENSE](file:///d:/workspace/dev/live2d_native_gui_viewer_mvp/LICENSE) 파일을 참고해 주십시오.
- **Live2D Cubism SDK & Core**:
  - Live2D Cubism SDK 및 Cubism Core의 저작권은 (주)Live2D (Live2D Inc.)에 있습니다.
  - 본 프로그램 소스 코드 저장소에는 저작권 및 라이선스 약관에 따라 **Live2D Cubism Core 라이브러리 및 SDK 프레임워크 소스 코드가 포함되어 있지 않습니다.**
  - 실제 렌더링을 연동하여 빌드하거나 상업적 배포를 하려면 반드시 Live2D 공식 웹사이트의 [Live2D Proprietary Software License Agreement](https://www.live2d.com/eula/)에 동의하고 Cubism SDK를 별도로 다운로드하여 연동해야 합니다.
- **기타 오픈소스**:
  - Dear ImGui (MIT License)
  - GLFW (zlib/libpng License)
  - GLEW (Modified BSD/MIT License)
  - FFmpeg (LGPL/GPL License)
  - 본 프로그램의 배포 및 활용 시에는 위 오픈소스 라이선스 및 가공하여 처리하는 Live2D 모델 에셋의 개별 라이선스 약관을 반드시 준수하여야 합니다. 상세한 내용은 [LICENSE_NOTES.md](file:///d:/workspace/dev/live2d_native_gui_viewer_mvp/LICENSE_NOTES.md)를 참고해 주십시오.
