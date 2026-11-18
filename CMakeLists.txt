# CocoaCandid builds in two ways:
#  - Inside an obs-studio source tree (how Windows releases are made, see .github/workflows/build.yml).
#    The tree's own libobs and plugins are built alongside, and OBS's packaging helpers bundle Qt,
#    FFmpeg and every plugin next to CocoaCandid.exe.
#  - On its own against an installed libobs (find_package(libobs)), for development on Linux.
cmake_minimum_required(VERSION 3.22)

set(CANDID_VERSION 0.1.2)

if(TARGET OBS::libobs)
  set(CANDID_IN_TREE ON)
  # OBS's helpers only bundle the dependencies of a target called obs-studio, so the app takes that name.
  set(CANDID_TARGET obs-studio)
else()
  set(CANDID_IN_TREE OFF)
  project(CocoaCandid VERSION ${CANDID_VERSION} LANGUAGES C CXX)
  set(CANDID_TARGET CocoaCandid)
  find_package(libobs REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

add_executable(${CANDID_TARGET})

target_sources(
  ${CANDID_TARGET}
  PRIVATE
    src/main.cpp
    src/App.cpp
    src/App.hpp
    src/Engine.cpp
    src/Engine.hpp
    src/Outputs.cpp
    src/Outputs.hpp
    src/Config.cpp
    src/Config.hpp
    src/Icons.cpp
    src/Icons.hpp
    src/Style.cpp
    src/Style.hpp
    src/MainWindow.cpp
    src/MainWindow.hpp
    src/Preview.cpp
    src/Preview.hpp
    src/SceneList.cpp
    src/SceneList.hpp
    src/SourceList.cpp
    src/SourceList.hpp
    src/PropertiesForm.cpp
    src/PropertiesForm.hpp
    src/Inspector.cpp
    src/Inspector.hpp
    src/Mixer.cpp
    src/Mixer.hpp
    src/AddSourceDialog.cpp
    src/AddSourceDialog.hpp
    src/AreaPicker.cpp
    src/AreaPicker.hpp
    src/SettingsDialog.cpp
    src/SettingsDialog.hpp
    src/Hotkeys.cpp
    src/Hotkeys.hpp
    src/Onboard.cpp
    src/Onboard.hpp
    src/Widgets.cpp
    src/Widgets.hpp
    resources/candid.qrc
)

target_compile_definitions(${CANDID_TARGET} PRIVATE CANDID_VERSION="${CANDID_VERSION}")
target_link_libraries(${CANDID_TARGET} PRIVATE OBS::libobs Qt::Widgets Qt::Svg)
set_target_properties(${CANDID_TARGET} PROPERTIES AUTOMOC ON AUTORCC ON)

# The graphics backend libobs loads, by file name.
if(WIN32)
  target_compile_definitions(${CANDID_TARGET} PRIVATE CANDID_GRAPHICS="libobs-d3d11")
else()
  target_compile_definitions(${CANDID_TARGET} PRIVATE CANDID_GRAPHICS="libobs-opengl")
endif()

if(WIN32)
  string(REPLACE "." "," CANDID_VERSION_COMMA "${CANDID_VERSION}")
  configure_file(resources/windows/candid.rc.in "${CMAKE_CURRENT_BINARY_DIR}/candid.rc" @ONLY)
  target_sources(${CANDID_TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/candid.rc" resources/windows/candid.manifest)
  target_link_libraries(${CANDID_TARGET} PRIVATE winmm dwmapi)
  set_target_properties(${CANDID_TARGET} PROPERTIES WIN32_EXECUTABLE TRUE)
  target_compile_definitions(${CANDID_TARGET} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

if(CANDID_IN_TREE)
  set_target_properties_obs(${CANDID_TARGET} PROPERTIES FOLDER frontend OUTPUT_NAME CocoaCandid)
  set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CANDID_TARGET})
  if(WIN32)
    set_target_properties(
      ${CANDID_TARGET}
      PROPERTIES
        VS_DEBUGGER_COMMAND "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/bin/64bit/CocoaCandid.exe"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/bin/64bit"
    )
  endif()
endif()
