cmake_minimum_required(VERSION 3.21)
project(L2DViewerGuiNativeMVP_R7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure MSVC compiles all source files as UTF-8 so Unicode string literals
# (play/pause/stop/cog symbols used in ImGui labels) are preserved correctly.
if(MSVC)
  add_compile_options(/utf-8)
endif()

option(L2D_USE_MOCK_BACKEND "Build with mock backend so the GUI can run without Cubism SDK" ON)
option(L2D_ENABLE_CUBISM_NATIVE "Enable Live2D Cubism Native backend" OFF)
set(IMGUI_DIR "" CACHE PATH "Path to Dear ImGui checkout, e.g. D:/workspace/dev/live2d_native_gui_viewer_mvp/third_party/imgui")
set(CUBISM_SDK_ROOT "" CACHE PATH "Path to Live2D CubismSdkForNative root, e.g. D:/workspace/dev/CubismSdkForNative-5-r.5")

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG QUIET)
find_package(GLEW QUIET)

if(NOT glfw3_FOUND)
  message(FATAL_ERROR "glfw3 not found. Install with vcpkg: vcpkg install glfw3:x64-windows glew:x64-windows, then configure with -DCMAKE_TOOLCHAIN_FILE=...")
endif()
if(NOT GLEW_FOUND)
  message(FATAL_ERROR "GLEW not found. Install with vcpkg: vcpkg install glew:x64-windows")
endif()
if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
  message(FATAL_ERROR "IMGUI_DIR is not set or does not point to Dear ImGui. Run scripts/fetch_imgui.ps1")
endif()

add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_library(l2d_backend SHARED
  src/backends/BackendFactory.cpp
  src/backends/MockBackend.cpp
  src/backends/CubismNativeBackend.cpp
  src/util/PathUtil.cpp
  src/util/CrashDiagnostics.cpp
)
target_include_directories(l2d_backend PRIVATE src)
target_compile_definitions(l2d_backend PRIVATE L2D_BACKEND_EXPORTS)
target_link_libraries(l2d_backend PRIVATE glfw GLEW::GLEW OpenGL::GL)
target_compile_definitions(l2d_backend PRIVATE
  $<$<BOOL:${L2D_USE_MOCK_BACKEND}>:L2D_USE_MOCK_BACKEND=1>
  $<$<BOOL:${L2D_ENABLE_CUBISM_NATIVE}>:L2D_ENABLE_CUBISM_NATIVE=1>
)

add_executable(l2d_viewer_gui
  src/main.cpp
  src/app/GuiApplication.cpp
  src/app/AppState.cpp
  src/gui/MainGui.cpp
  src/render/ViewportCamera.cpp
  src/render/ModelScanner.cpp
  src/export/FrameExporter.cpp
  src/util/ProcessPipe.cpp
  src/util/PathUtil.cpp
  src/util/CrashDiagnostics.cpp
  src/util/NativeDialogs.cpp
  src/gui/ExportDialogs.cpp
  src/gui/ConsoleLog.cpp
  src/util/ProjectSerializer.cpp
  src/app/UndoManager.cpp
)

find_package(Stb REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_include_directories(l2d_viewer_gui PRIVATE src ${Stb_INCLUDE_DIR})
target_link_libraries(l2d_viewer_gui PRIVATE imgui glfw GLEW::GLEW OpenGL::GL l2d_backend nlohmann_json::nlohmann_json)

target_compile_definitions(l2d_viewer_gui PRIVATE
  $<$<BOOL:${L2D_USE_MOCK_BACKEND}>:L2D_USE_MOCK_BACKEND=1>
  $<$<BOOL:${L2D_ENABLE_CUBISM_NATIVE}>:L2D_ENABLE_CUBISM_NATIVE=1>
)

if(MSVC)
  set_target_properties(l2d_viewer_gui PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

# Add every subdirectory under a tree as an include directory.  Cubism Native
# sample headers such as LAppAllocator_Common.hpp and LAppTextureManager_Common.hpp
# move between Samples/Common/include, Samples/Common/src, and nested folders
# depending on SDK release, so a flat include path is not enough.
function(l2d_collect_include_dirs out_var root_dir)
  set(_dirs "")
  if(EXISTS "${root_dir}")
    list(APPEND _dirs "${root_dir}")
    file(GLOB_RECURSE _children LIST_DIRECTORIES true CONFIGURE_DEPENDS "${root_dir}/*")
    foreach(_child IN LISTS _children)
      if(IS_DIRECTORY "${_child}")
        list(APPEND _dirs "${_child}")
      endif()
    endforeach()
  endif()
  set(${out_var} ${_dirs} PARENT_SCOPE)
endfunction()

if(L2D_ENABLE_CUBISM_NATIVE)
  if(L2D_USE_MOCK_BACKEND)
    message(FATAL_ERROR "Native mode requires -DL2D_USE_MOCK_BACKEND=OFF")
  endif()
  if(NOT EXISTS "${CUBISM_SDK_ROOT}/Core/include")
    message(FATAL_ERROR "CUBISM_SDK_ROOT is wrong: ${CUBISM_SDK_ROOT}/Core/include not found")
  endif()
  if(NOT EXISTS "${CUBISM_SDK_ROOT}/Framework/src")
    message(FATAL_ERROR "CUBISM_SDK_ROOT is wrong: ${CUBISM_SDK_ROOT}/Framework/src not found")
  endif()
  if(NOT EXISTS "${CUBISM_SDK_ROOT}/Samples/OpenGL/Demo/proj.win.cmake/src/LAppModel.cpp")
    message(FATAL_ERROR "OpenGL Demo sources not found under CUBISM_SDK_ROOT")
  endif()

  file(GLOB_RECURSE CUBISM_FRAMEWORK_SOURCES CONFIGURE_DEPENDS
    "${CUBISM_SDK_ROOT}/Framework/src/*.cpp"
  )
  # This GUI uses the OpenGL renderer only. Do not compile D3D9/D3D11/Vulkan/Metal
  # backends; otherwise the build requires d3dx9.h or vulkan/vulkan.h.
  list(FILTER CUBISM_FRAMEWORK_SOURCES EXCLUDE REGEX "[/\\]Rendering[/\\](D3D9|D3D11|Vulkan|Metal)[/\\]")
  # Cubism SDK 5.x sample-common sources are not always under Samples/Common/src.
  # Some releases place files such as LAppAllocator_Common.cpp,
  # LAppModel_Common.cpp, LAppTextureManager_Common.cpp, and
  # LAppWavFileHandler_Common.cpp directly under Samples/Common or below
  # per-platform subdirectories.  Collect recursively from the whole Common tree.
  file(GLOB_RECURSE CUBISM_SAMPLE_COMMON_SOURCES CONFIGURE_DEPENDS
    "${CUBISM_SDK_ROOT}/Samples/Common/*.cpp"
    "${CUBISM_SDK_ROOT}/Samples/Common/**/*.cpp"
  )
  # Exclude possible platform-specific entry points if an SDK release adds them.
  list(FILTER CUBISM_SAMPLE_COMMON_SOURCES EXCLUDE REGEX "[/\\]main\\.cpp$")

  l2d_collect_include_dirs(CUBISM_SAMPLE_COMMON_INCLUDE_DIRS "${CUBISM_SDK_ROOT}/Samples/Common")
  l2d_collect_include_dirs(CUBISM_OPENGL_DEMO_INCLUDE_DIRS "${CUBISM_SDK_ROOT}/Samples/OpenGL/Demo/proj.win.cmake/src")
  set(CUBISM_SAMPLE_OPENGL_SOURCES
    "${CUBISM_SDK_ROOT}/Samples/OpenGL/Demo/proj.win.cmake/src/LAppDefine.cpp"
    "${CUBISM_SDK_ROOT}/Samples/OpenGL/Demo/proj.win.cmake/src/LAppModel.cpp"
    "${CUBISM_SDK_ROOT}/Samples/OpenGL/Demo/proj.win.cmake/src/LAppTextureManager.cpp"
  )

  target_sources(l2d_backend PRIVATE
    src/backends/CubismNativeSamplesBridge.cpp
    src/cubism_shim/LAppDelegateShim.cpp
    src/cubism_shim/LAppPal.cpp
    ${CUBISM_FRAMEWORK_SOURCES}
    ${CUBISM_SAMPLE_COMMON_SOURCES}
    ${CUBISM_SAMPLE_OPENGL_SOURCES}
  )

  target_include_directories(l2d_backend PRIVATE
    src/cubism_shim
    "${CUBISM_SDK_ROOT}/Core/include"
    "${CUBISM_SDK_ROOT}/Framework/src"
    ${CUBISM_SAMPLE_COMMON_INCLUDE_DIRS}
    ${CUBISM_OPENGL_DEMO_INCLUDE_DIRS}
    "${CUBISM_SDK_ROOT}/Samples/OpenGL/thirdParty/stb"
    "${CUBISM_SDK_ROOT}/Samples/OpenGL/thirdParty/stb/include"
    "${CUBISM_SDK_ROOT}/Samples/thirdParty/stb"
    "${CUBISM_SDK_ROOT}/thirdParty/stb"
  )

  list(LENGTH CUBISM_SAMPLE_COMMON_SOURCES CUBISM_SAMPLE_COMMON_SOURCE_COUNT)
  message(STATUS "Cubism sample common include dirs: ${CUBISM_SAMPLE_COMMON_INCLUDE_DIRS}")
  message(STATUS "Cubism sample common source count: ${CUBISM_SAMPLE_COMMON_SOURCE_COUNT}")

  # Cubism SDK package layouts vary between releases.  Do not assume
  # Core/lib/windows/x86_64/143 only; search recursively under Core/lib/windows.
  set(CUBISM_CORE_LIB "" CACHE FILEPATH "Optional explicit path to Live2DCubismCore .lib")
  set(CUBISM_CORE_DLL "" CACHE FILEPATH "Optional explicit path to Live2DCubismCore .dll")

  if(CUBISM_CORE_LIB AND EXISTS "${CUBISM_CORE_LIB}")
    set(LIVE2D_CUBISM_CORE_LIB "${CUBISM_CORE_LIB}")
  else()
    file(GLOB_RECURSE LIVE2D_CUBISM_CORE_LIB_CANDIDATES CONFIGURE_DEPENDS
      "${CUBISM_SDK_ROOT}/Core/lib/windows/*.lib"
    )
    set(LIVE2D_CUBISM_CORE_LIB_MATCHES "")
    foreach(_lib IN LISTS LIVE2D_CUBISM_CORE_LIB_CANDIDATES)
      get_filename_component(_lib_name "${_lib}" NAME)
      if(_lib_name MATCHES "^Live2DCubismCore.*\.lib$")
        list(APPEND LIVE2D_CUBISM_CORE_LIB_MATCHES "${_lib}")
      endif()
    endforeach()

    list(LENGTH LIVE2D_CUBISM_CORE_LIB_MATCHES _core_lib_count)
    if(_core_lib_count EQUAL 0)
      message(FATAL_ERROR
        "Could not find Live2DCubismCore .lib under ${CUBISM_SDK_ROOT}/Core/lib/windows.
"
        "Run this in PowerShell to see the actual files:
"
        "  Get-ChildItem -Recurse '${CUBISM_SDK_ROOT}/Core/lib/windows' -Filter '*CubismCore*.lib'
"
        "Then re-run CMake with -DCUBISM_CORE_LIB=<full path to .lib> if needed."
      )
    endif()

    # Prefer the x86_64 import library because this project is generated as -A x64.
    # Cubism SDK layouts differ by release: some use 141/142/143 folders and some
    # name the import lib Live2DCubismCore_MD.lib.
    list(GET LIVE2D_CUBISM_CORE_LIB_MATCHES 0 LIVE2D_CUBISM_CORE_LIB)
    foreach(_lib IN LISTS LIVE2D_CUBISM_CORE_LIB_MATCHES)
      get_filename_component(_lib_name "${_lib}" NAME)
      if(_lib MATCHES "[/\\]x86_64[/\\]" AND _lib_name MATCHES "^Live2DCubismCore(_MD)?\\.lib$")
        set(LIVE2D_CUBISM_CORE_LIB "${_lib}")
      endif()
    endforeach()
    foreach(_lib IN LISTS LIVE2D_CUBISM_CORE_LIB_MATCHES)
      get_filename_component(_lib_name "${_lib}" NAME)
      if(_lib MATCHES "[/\\]x86_64[/\\].*[/\\]143[/\\]" AND _lib_name MATCHES "^Live2DCubismCore(_MD)?\\.lib$")
        set(LIVE2D_CUBISM_CORE_LIB "${_lib}")
      endif()
    endforeach()
  endif()

  if(CUBISM_CORE_DLL AND EXISTS "${CUBISM_CORE_DLL}")
    set(LIVE2D_CUBISM_CORE_DLL "${CUBISM_CORE_DLL}")
  else()
    file(GLOB_RECURSE LIVE2D_CUBISM_CORE_DLL_CANDIDATES CONFIGURE_DEPENDS
      "${CUBISM_SDK_ROOT}/Core/dll/windows/*.dll"
    )
    set(LIVE2D_CUBISM_CORE_DLL_MATCHES "")
    foreach(_dll IN LISTS LIVE2D_CUBISM_CORE_DLL_CANDIDATES)
      get_filename_component(_dll_name "${_dll}" NAME)
      if(_dll_name MATCHES "^Live2DCubismCore.*\.dll$")
        list(APPEND LIVE2D_CUBISM_CORE_DLL_MATCHES "${_dll}")
      endif()
    endforeach()

    list(LENGTH LIVE2D_CUBISM_CORE_DLL_MATCHES _core_dll_count)
    if(_core_dll_count EQUAL 0)
      message(FATAL_ERROR
        "Could not find Live2DCubismCore .dll under ${CUBISM_SDK_ROOT}/Core/dll/windows.
"
        "Run this in PowerShell to see the actual files:
"
        "  Get-ChildItem -Recurse '${CUBISM_SDK_ROOT}/Core/dll/windows' -Filter '*CubismCore*.dll'
"
        "Then re-run CMake with -DCUBISM_CORE_DLL=<full path to .dll> if needed."
      )
    endif()

    list(GET LIVE2D_CUBISM_CORE_DLL_MATCHES 0 LIVE2D_CUBISM_CORE_DLL)
    foreach(_dll IN LISTS LIVE2D_CUBISM_CORE_DLL_MATCHES)
      get_filename_component(_dll_name "${_dll}" NAME)
      if(_dll MATCHES "[/\\]x86_64[/\\]" AND _dll_name STREQUAL "Live2DCubismCore.dll")
        set(LIVE2D_CUBISM_CORE_DLL "${_dll}")
      endif()
    endforeach()
  endif()

  message(STATUS "Using Live2D Cubism Core lib: ${LIVE2D_CUBISM_CORE_LIB}")
  message(STATUS "Using Live2D Cubism Core dll: ${LIVE2D_CUBISM_CORE_DLL}")

  target_link_libraries(l2d_backend PRIVATE ${LIVE2D_CUBISM_CORE_LIB})
  target_compile_definitions(l2d_backend PRIVATE CSM_TARGET_WIN_GL GLFW_INCLUDE_NONE)

  add_custom_command(TARGET l2d_viewer_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${LIVE2D_CUBISM_CORE_DLL}"
      "$<TARGET_FILE_DIR:l2d_viewer_gui>/Live2DCubismCore.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CUBISM_SDK_ROOT}/Framework/src/Rendering/OpenGL/Shaders/Standard"
      "$<TARGET_FILE_DIR:l2d_viewer_gui>/FrameworkShaders"
  )
endif()

if(MSVC)
  target_compile_options(l2d_viewer_gui PRIVATE /W4 /permissive- /utf-8)
  target_compile_options(l2d_backend PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(l2d_viewer_gui PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(l2d_backend PRIVATE -Wall -Wextra -Wpedantic)
endif()
