cmake_minimum_required(VERSION 3.16)

project(breakpad)

include_directories(src)

add_library(breakpad_client STATIC src/client/minidump_file_writer.cc)
target_compile_features(breakpad_client PRIVATE c_std_11 cxx_std_11)

target_sources(breakpad_client PRIVATE
    	src/common/convert_UTF.cc
    	src/common/convert_UTF.h
    	src/common/md5.h
    	src/common/md5.cc
    	src/common/string_conversion.cc
    	src/common/string_conversion.h)

if(APPLE)
	enable_language(OBJC)
	set_target_properties(breakpad_client PROPERTIES
	    FRAMEWORK TRUE
	    FRAMEWORK_VERSION C
	    MACOSX_FRAMEWORK_IDENTIFIER com.googlecode.google-breakpad
	    MACOSX_FRAMEWORK_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/src/client/mac/Framework/Info.plist
	    # "current version" in semantic format in Mach-O binary file
	    VERSION 16.4.0
	    # "compatibility version" in semantic format in Mach-O binary file
	    SOVERSION 1.0.0
	    PUBLIC_HEADER src/client/mac/Framework/Breakpad.h
	    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
	target_include_directories(breakpad_client PRIVATE
		src/third_party/mac/include
		src/client/apple/Framework
		src/common/mac)
	add_compile_definitions(HAVE_MACH_O_NLIST_H)
	target_sources(breakpad_client PRIVATE
		src/common/mac/MachIPC.mm
		src/common/mac/bootstrap_compat.cc
		src/common/mac/file_id.cc
		src/common/mac/launch_reporter.h
		src/common/mac/launch_reporter.cc
		src/common/mac/macho_id.cc
		src/common/mac/macho_utilities.cc
		src/common/mac/macho_walker.cc
		src/common/mac/string_utilities.cc

		src/client/mac/crash_generation/crash_generation_client.h
		src/client/mac/crash_generation/crash_generation_client.cc
		src/client/mac/crash_generation/ConfigFile.h
		src/client/mac/crash_generation/ConfigFile.mm
		src/client/mac/crash_generation/Inspector.h
		src/client/mac/crash_generation/Inspector.mm
		src/client/mac/crash_generation/InspectorMain.mm
		src/client/mac/crash_generation/crash_generation_server.cc
		src/client/mac/handler/breakpad_nlist_64.cc
		src/client/mac/handler/dynamic_images.cc
		src/client/mac/handler/exception_handler.cc
		src/client/mac/handler/minidump_generator.cc
		src/client/mac/handler/protected_memory_allocator.cc
		src/client/mac/handler/protected_memory_allocator.h
		src/client/mac/Framework/Breakpad.h
		src/client/mac/Framework/Breakpad.mm
		src/client/mac/Framework/Breakpad_Prefix.pch
		src/client/mac/Framework/OnDemandServer.h
		src/client/mac/Framework/OnDemandServer.mm

#	add_executable(dump_syms
#		src/common/dwarf/bytereader.cc
#		src/common/dwarf/dwarf2diehandler.cc
#		src/common/dwarf/dwarf2reader.cc
#		src/common/dwarf_cfi_to_module.cc
#		src/common/dwarf_cu_to_module.cc
#		src/common/dwarf_line_to_module.cc
#		src/common/language.cc
#		src/common/mac/dump_syms.mm
#		src/common/mac/macho_reader.cc
#		src/common/module.cc
#		src/common/stabs_reader.cc
#		src/common/stabs_to_module.cc
#		src/tools/mac/dump_syms/dump_syms_tool.mm)
	)

elseif(UNIX)
    enable_language(ASM)
    if(ANDROID)
		target_include_directories(breakpad_client PRIVATE src/common/android/include)
    endif()
    target_sources(breakpad_client PRIVATE
        src/client/linux/crash_generation/crash_generation_client.cc
		src/client/linux/crash_generation/crash_generation_server.cc
        src/client/linux/dump_writer_common/thread_info.cc
        src/client/linux/dump_writer_common/ucontext_reader.cc
        src/client/linux/handler/exception_handler.h
        src/client/linux/handler/exception_handler.cc
        src/client/linux/handler/minidump_descriptor.cc
    	src/client/linux/handler/minidump_descriptor.h
    	src/client/linux/log/log.cc
    	src/client/linux/log/log.h
    	src/client/linux/microdump_writer/microdump_writer.cc
    	src/client/linux/microdump_writer/microdump_writer.h
		src/client/linux/minidump_writer/linux_core_dumper.cc
		src/client/linux/minidump_writer/linux_core_dumper.h
    	src/client/linux/minidump_writer/linux_dumper.cc
    	src/client/linux/minidump_writer/linux_dumper.h
    	src/client/linux/minidump_writer/linux_ptrace_dumper.cc
    	src/client/linux/minidump_writer/linux_ptrace_dumper.h
    	src/client/linux/minidump_writer/minidump_writer.cc
    	src/client/linux/minidump_writer/minidump_writer.h
    	src/client/minidump_file_writer-inl.h
    	src/client/minidump_file_writer.h

    	src/common/linux/breakpad_getcontext.S
		src/common/linux/elf_core_dump.cc
		src/common/linux/elf_core_dump.h
    	src/common/linux/elfutils.cc
    	src/common/linux/elfutils.h
    	src/common/linux/file_id.cc
    	src/common/linux/file_id.h
    	src/common/linux/guid_creator.cc
    	src/common/linux/guid_creator.h
    	src/common/linux/linux_libc_support.cc
    	src/common/linux/memory_mapped_file.cc
    	src/common/linux/safe_readlink.cc)

	if(NOT ANDROID)
		add_executable(dump_syms
			src/common/dwarf_cfi_to_module.cc
			src/common/dwarf_cu_to_module.cc
			src/common/dwarf_line_to_module.cc
			src/common/dwarf_range_list_handler.cc
			src/common/language.cc
			src/common/module.cc
			src/common/path_helper.cc
			src/common/stabs_reader.cc
			src/common/stabs_to_module.cc
			src/common/dwarf/bytereader.cc
			src/common/dwarf/dwarf2diehandler.cc
			src/common/dwarf/dwarf2reader.cc
			src/common/dwarf/elf_reader.cc
			src/common/linux/crc32.cc
			src/common/linux/dump_symbols.cc
			src/common/linux/dump_symbols.h
			src/common/linux/elf_symbols_to_module.cc
			src/common/linux/elf_symbols_to_module.h
			src/common/linux/elfutils.cc
			src/common/linux/file_id.cc
			src/common/linux/linux_libc_support.cc
			src/common/linux/memory_mapped_file.cc
			src/common/linux/safe_readlink.cc
			src/tools/linux/dump_syms/dump_syms.cc)
		target_compile_definitions(dump_syms PRIVATE HAVE_A_OUT_H)
	endif()
endif()
