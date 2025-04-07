# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
#
# SPDX-License-Identifier: BSD-3-Clause

project(
	'xbyak',
	'cpp',
	version: '7.23.1',
	license: 'BSD-3-Clause',
	default_options: 'b_ndebug=if-release'
)

install_subdir('xbyak', install_dir: get_option('includedir'))

xbyak_dep = declare_dependency(include_directories: include_directories('.'))

if meson.version().version_compare('>=0.54.0')
	meson.override_dependency(meson.project_name(), xbyak_dep)
endif

import('pkgconfig').generate(
	name: meson.project_name(),
	description: 'JIT assembler for x86(IA32), x64(AMD64, x86-64)',
	version: meson.project_version(),
	url: 'https://github.com/herumi/xbyak'
)

if meson.version().version_compare('>=0.50.0')
	cmake = import('cmake')

	cmake.write_basic_package_version_file(
		name: meson.project_name(),
		version: meson.project_version()
	)

	cmake_conf = configuration_data()
	cmake_conf.set('TARGET_NAME', meson.project_name() + '::' + meson.project_name())
	cmake_conf.set('ABSOLUTE_INCLUDE_DIR', get_option('prefix')/get_option('includedir'))

	cmake.configure_package_config_file(
		name: meson.project_name(),
		input: 'cmake'/'meson-config.cmake.in',
		configuration: cmake_conf
	)
endif
