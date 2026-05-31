# cmake/finders/Findlibobs.cmake
#
# Minimal fallback finder for libobs. The preferred path is the OBS-shipped
# libobsConfig.cmake (find_package(libobs CONFIG REQUIRED) — see the top-level
# CMakeLists.txt). This file only runs when that package config is missing,
# which is common on dev machines that haven't `make install`'d obs-studio.
#
# Usage:
#   - Install OBS Studio from source per obs-studio/wiki, OR
#   - Set -DLIBOBS_PATH=/path/to/libobs/install when configuring this plugin.
#
# Emits OBS::libobs as an imported target on success.

include(FindPackageHandleStandardArgs)

if(DEFINED LIBOBS_PATH)
	set(_libobs_hint "${LIBOBS_PATH}")
elseif(DEFINED ENV{LIBOBS_PATH})
	set(_libobs_hint "$ENV{LIBOBS_PATH}")
endif()

find_path(LIBOBS_INCLUDE_DIR
	NAMES obs-module.h
	HINTS ${_libobs_hint}
	PATH_SUFFIXES include include/obs include/libobs)

find_library(LIBOBS_LIBRARY
	NAMES obs libobs
	HINTS ${_libobs_hint}
	PATH_SUFFIXES lib lib64 bin)

# pkg-config fallback (Linux mostly).
if(NOT LIBOBS_INCLUDE_DIR OR NOT LIBOBS_LIBRARY)
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(PC_LIBOBS QUIET libobs)
		if(PC_LIBOBS_FOUND)
			set(LIBOBS_INCLUDE_DIR ${PC_LIBOBS_INCLUDE_DIRS})
			set(LIBOBS_LIBRARY ${PC_LIBOBS_LIBRARIES})
		endif()
	endif()
endif()

find_package_handle_standard_args(libobs
	REQUIRED_VARS LIBOBS_LIBRARY LIBOBS_INCLUDE_DIR
	FAIL_MESSAGE
		"libobs not found. Install OBS Studio from source (see "
		"https://github.com/obsproject/obs-studio/wiki/Install-Instructions) "
		"or pass -DLIBOBS_PATH=/path/to/libobs/install when configuring.")

if(libobs_FOUND AND NOT TARGET OBS::libobs)
	add_library(OBS::libobs UNKNOWN IMPORTED)
	set_target_properties(OBS::libobs PROPERTIES
		IMPORTED_LOCATION "${LIBOBS_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${LIBOBS_INCLUDE_DIR}")
endif()

mark_as_advanced(LIBOBS_INCLUDE_DIR LIBOBS_LIBRARY)
