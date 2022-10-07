find_path(LuaBridge_INCLUDE_DIR LuaBridge/LuaBridge.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LuaBridge
	REQUIRED_VARS LuaBridge_INCLUDE_DIR
)

if (LuaBridge_FOUND AND NOT TARGET LuaBridge::LuaBridge)
	add_library(LuaBridge::LuaBridge INTERFACE IMPORTED)
	set_target_properties(LuaBridge::LuaBridge PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${LuaBridge_INCLUDE_DIR}"
	)
endif()

mark_as_advanced(LuaBridge_INCLUDE_DIR)
