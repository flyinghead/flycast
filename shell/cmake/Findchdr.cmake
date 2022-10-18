find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_search_module(CHDR QUIET IMPORTED_TARGET GLOBAL libchdr)
	if (CHDR_FOUND)
		add_library(chdr::chdr ALIAS PkgConfig::CHDR)
		set(chdr_VERSION ${CHDR_VERSION})
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(chdr
	REQUIRED_VARS CHDR_LINK_LIBRARIES
	VERSION_VAR chdr_VERSION
)
