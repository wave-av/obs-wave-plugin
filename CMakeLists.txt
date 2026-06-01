# obs-wave-plugin — top-level CMake build.
#
# Resolves libobs via find_package(libobs CONFIG REQUIRED) — the OBS plugin
# template's recommended flow. Never vendor libobs headers or binaries.
# A fallback finder lives at cmake/finders/Findlibobs.cmake for dev
# machines that don't have libobsConfig.cmake in their CMAKE_PREFIX_PATH.

cmake_minimum_required(VERSION 3.28...3.31)

project(wave-output
	VERSION 0.1.0
	DESCRIPTION "WAVE Streaming Service output plugin for OBS Studio"
	HOMEPAGE_URL "https://github.com/wave-av/obs-wave-plugin"
	LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/finders")

# Resolve libobs. Try the OBS-provided package config first; fall back to
# our minimal finder which emits a clear "set LIBOBS_PATH" diagnostic.
find_package(libobs CONFIG QUIET)
if(NOT libobs_FOUND)
	find_package(libobs MODULE REQUIRED)
endif()

add_library(wave-output MODULE
	src/plugin-main.c
	src/wave-output.c
	src/wave-properties.c)

target_include_directories(wave-output PRIVATE src)

target_link_libraries(wave-output PRIVATE OBS::libobs)

set_target_properties(wave-output PROPERTIES
	PREFIX ""
	OUTPUT_NAME "wave-output"
	FOLDER "plugins")

# Locale ships alongside the plugin binary so obs_module_text() can find it.
install(FILES data/locale/en-US.ini
	DESTINATION "wave-output/data/locale")

install(TARGETS wave-output
	LIBRARY DESTINATION "wave-output/bin/64bit"
	RUNTIME DESTINATION "wave-output/bin/64bit")
