find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_search_module(XXHASH QUIET IMPORTED_TARGET GLOBAL libxxhash)
	if (XXHASH_FOUND)
		add_library(xxHash::xxhash ALIAS PkgConfig::XXHASH)
		set(xxHash_VERSION ${XXHASH_VERSION})
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(xxHash
	REQUIRED_VARS XXHASH_LINK_LIBRARIES
	VERSION_VAR xxHash_VERSION
)
