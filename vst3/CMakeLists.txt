cmake_minimum_required(VERSION 3.20)

# Usage: cmake -S vst3 -B build-vst3 -DVST3_SDK_ROOT=/path/to/vst3sdk
set(VST3_SDK_ROOT "" CACHE PATH "Path to the Steinberg VST3 SDK (the folder containing pluginterfaces/, public.sdk/, base/)")
if(NOT VST3_SDK_ROOT OR NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "Set -DVST3_SDK_ROOT=<path to vst3sdk> (a checkout of github.com/steinbergmedia/vst3sdk with submodules).")
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal build on macOS")
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "No custom editor yet")   # generic host editor only
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "")                           # do not install into the user's plug-in folder

project(CarrierFieldVst3 VERSION 1.0.0.1 LANGUAGES CXX)

add_subdirectory(${VST3_SDK_ROOT} ${CMAKE_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(CarrierFieldVst3
    source/cids.h
    source/version.h
    source/CarrierFieldVst3Bridge.h
    source/CarrierFieldProcessor.h
    source/CarrierFieldProcessor.cpp
    source/CarrierFieldController.h
    source/CarrierFieldController.cpp
    source/entry.cpp
)

# The DSP is header-only and lives one level up; the code is C++17-clean (also builds as C++20).
target_include_directories(CarrierFieldVst3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_link_libraries(CarrierFieldVst3 PRIVATE sdk)
smtg_target_configure_version_file(CarrierFieldVst3)

if(SMTG_MAC)
    smtg_target_set_bundle(CarrierFieldVst3 BUNDLE_IDENTIFIER com.cosmicdsp.carrierfield.vst3 COMPANY_NAME "Cosmic DSP")
elseif(SMTG_WIN)
    target_sources(CarrierFieldVst3 PRIVATE resource/win32resource.rc)
endif()
