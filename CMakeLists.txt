cmake_minimum_required(VERSION 3.15)
project(SvenderBass VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED VST3_SDK_ROOT)
  message(FATAL_ERROR "Set -DVST3_SDK_ROOT to your VST3 SDK root")
endif()

# The SDK root must contain the VST3 SDK's top-level CMakeLists.txt
set(vst3sdk_SOURCE_DIR "${VST3_SDK_ROOT}")
add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

set(SRC
  source/factory.cpp
  source/processor.cpp
  source/controller.cpp
  source/editor.cpp
)

set(HDR
  source/ids.h
  source/version.h
  source/processor.h
  source/controller.h
  source/dsp.h
  source/editor.h
)

# Create the VST3 plugin target
smtg_add_vst3plugin_with_pkgname(SvenderBass "SvenderBass" ${SRC} ${HDR})

target_include_directories(SvenderBass PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_link_libraries(SvenderBass PRIVATE sdk)

# Add your faceplate PNGs as plugin resources (this is where RESOURCES belongs)
smtg_target_add_plugin_resources(SvenderBass
  RESOURCES
    "resource/faceplate.png"
    "resource/faceplate@2x.png"
)

if (SMTG_WIN)
  set(_SvenderBass_bin_dir "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/SvenderBass.vst3/Contents/x86_64-win")

  set_target_properties(SvenderBass PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_SvenderBass_bin_dir}"
    LIBRARY_OUTPUT_DIRECTORY "${_SvenderBass_bin_dir}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
    OUTPUT_NAME "SvenderBass"
    SUFFIX ".vst3"
  )
endif()
