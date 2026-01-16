#ifndef LIBRETRO
#include "types.h"
#include "emulator.h"
#include "hw/mem/addrspace.h"
#include "cfg/cfg.h"
#include "cfg/option.h"
#include "log/LogManager.h"
#include "ui/gui.h"
#include "oslib/oslib.h"
#include "oslib/directory.h"
#include "oslib/storage.h"
#include "debug/gdb_server.h"
#include "archive/rzip.h"
#include "ui/mainui.h"
#include "input/gamepad_device.h"
#include "lua/lua.h"
#include "stdclass.h"
#include "serialize.h"
#include "oslib/i18n.h"
#include "input/maplelink.h"
#include <time.h>
#ifdef TARGET_UWP
#include <winrt/Windows.System.h>
#include <winrt/Windows.Foundation.h>
#endif

static std::string lastStateFile;
static time_t lastStateTime;

struct SavestateHeader
{
	void init()
	{
		memcpy(magic, MAGIC, sizeof(magic));
		creationDate = time(nullptr);
		version = Deserializer::Current;
		pngSize = 0;
	}

	bool isValid() const {
		return !memcmp(magic, MAGIC, sizeof(magic));
	}

	char magic[8];
	u64 creationDate;
	u32 version;
	u32 pngSize;
	// png data

	static constexpr const char *MAGIC = "FLYSAVE1";
};

int flycast_init(int argc, char* argv[])
{
#if defined(TEST_AUTOMATION)
	setbuf(stdout, 0);
	setbuf(stderr, 0);
	settings.aica.muteAudio = true;
#endif
	try {
		if (!addrspace::reserve())
		{
			ERROR_LOG(VMEM, "Failed to alloc mem");
			return -1;
		}
		config::parseCommandLine(argc, argv);
		if (config::loadInt("naomi", "BoardId") != 0)
		{
			settings.naomi.multiboard = true;
			settings.naomi.slave = true;
		}
		settings.naomi.drivingSimSlave = config::loadInt("naomi", "DrivingSimSlave");

		config::Settings::instance().reset();
		LogManager::Shutdown();
		if (!config::open())
		{
			LogManager::Init();
			NOTICE_LOG(BOOT, "Config directory is not set. Starting onboarding");
			gui_open_onboarding();
		}
		else
		{
			LogManager::Init();
			config::Settings::instance().load(false);
		}
		gui_init();
		os_CreateWindow();
		os_SetupInput();

		if(config::GDB)
			debugger::init(config::GDBPort);
		lua::init();

		if(config::ProfilerEnabled)
			LogManager::GetInstance()->SetEnable(LogTypes::PROFILER, true);

		return 0;
	} catch (const std::exception& e) {
		ERROR_LOG(BOOT, "flycast_init failed: %s", e.what());
		return 1;
	} catch (...) {
		ERROR_LOG(BOOT, "flycast_init: unknown exception");
		return 1;
	}
}

#ifndef __ANDROID__
void dc_exit()
{
	try {
		emu.unloadGame();
	} catch (...) { }
	mainui_stop();

#ifdef TARGET_UWP
	extern std::string launchOnExitUri;
	if (!launchOnExitUri.empty())
	{
		INFO_LOG(BOOT, "Launching exit URI: %s", launchOnExitUri.c_str());
		try {
			using namespace winrt::Windows::System;
			using namespace winrt::Windows::Foundation;

			auto wUri = winrt::to_hstring(launchOnExitUri);
			Uri uri(wUri);
			auto asyncOp = Launcher::LaunchUriAsync(uri);
			asyncOp.get();
		} catch (...) {
			ERROR_LOG(BOOT, "Failed to launch exit URI");
		}
	}
#endif
}
#endif

void SaveSettings()
{
	config::Settings::instance().save();
	GamepadDevice::SaveMaplePorts();

#ifdef __ANDROID__
	void SaveAndroidSettings();
	SaveAndroidSettings();
#endif
	LogManager::GetInstance()->UpdateConfig();
}

void flycast_term()
{
	gui_cancel_load();
	lua::term();
	emu.term();
	os_DestroyWindow();
	gui_term();
	os_TermInput();
}

bool dc_savestateAllowed() {
	return !settings.content.path.empty() && !settings.network.online
			&& !settings.naomi.multiboard && !MapleLink::StorageEnabled();
}

void dc_savestate(int index, const u8 *pngData, u32 pngSize)
{
	if (!dc_savestateAllowed())
		return;

	lastStateFile.clear();

	Serializer ser;
	dc_serialize(ser);

	void *data = malloc(ser.size());
	if (data == nullptr)
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not malloc %d bytes", (int)ser.size());
		os_notify(i18n::T("Save state failed - memory full"), 5000);
    	return;
	}

	ser = Serializer(data, ser.size());
	dc_serialize(ser);

	std::string filename = hostfs::getSavestatePath(index, true);
	FILE *f = nowide::fopen(filename.c_str(), "wb");
	if (f == nullptr)
	{
		WARN_LOG(SAVESTATE, "Failed to save state - could not open %s for writing", filename.c_str());
		os_notify(i18n::T("Cannot open save file"), 5000);
		free(data);
    	return;
	}

	RZipFile zipFile;
	SavestateHeader header;
	header.init();
	header.pngSize = pngSize;
	if (std::fwrite(&header, sizeof(header), 1, f) != 1)
		goto fail;
	if (pngSize > 0 && std::fwrite(pngData, 1, pngSize, f) != pngSize)
		goto fail;

#if 0
	// Uncompressed savestate
	std::fwrite(data, 1, ser.size(), f);
	std::fclose(f);
#else
	if (!zipFile.Open(f, true))
		goto fail;
	if (zipFile.Write(data, ser.size()) != ser.size())
		goto fail;
	zipFile.Close();
#endif

	free(data);
	NOTICE_LOG(SAVESTATE, "Saved state to %s size %d", filename.c_str(), (int)ser.size());
	os_notify(i18n::T("State saved"), 2000);
	return;

fail:
	WARN_LOG(SAVESTATE, "Failed to save state - error writing %s", filename.c_str());
	os_notify(i18n::T("Error saving state"), 5000);
	if (zipFile.rawFile() != nullptr)
		zipFile.Close();
	else
		std::fclose(f);
	free(data);
	// delete failed savestate?
}

void dc_loadstate(int index)
{
	if (!dc_savestateAllowed() || settings.raHardcoreMode)
		return;
	u32 total_size = 0;

	std::string filename = hostfs::getSavestatePath(index, false);
	FILE *f = hostfs::storage().openFile(filename, "rb");
	if (f == nullptr)
	{
		WARN_LOG(SAVESTATE, "Failed to load state - could not open %s for reading", filename.c_str());
		os_notify(i18n::T("Save state not found"), 2000);
		return;
	}
	SavestateHeader header;
	if (std::fread(&header, sizeof(header), 1, f) == 1)
	{
		if (!header.isValid())
			// seek to beginning of file if this isn't a valid header (legacy savestate)
			std::fseek(f, 0, SEEK_SET);
		else
			// skip png data
			std::fseek(f, header.pngSize, SEEK_CUR);
	}
	else {
		// probably not a valid savestate but we'll fail later
		std::fseek(f, 0, SEEK_SET);
	}

	if (index == -1 && config::GGPOEnable)
	{
		long pos = std::ftell(f);
		MD5Sum().add(f)
				.getDigest(settings.network.md5.savestate);
		std::fseek(f, pos, SEEK_SET);
	}
	RZipFile zipFile;
	if (zipFile.Open(f, false)) {
		total_size = (u32)zipFile.Size();
	}
	else
	{
		long pos = std::ftell(f);
		std::fseek(f, 0, SEEK_END);
		total_size = (u32)std::ftell(f) - pos;
		std::fseek(f, pos, SEEK_SET);
	}
	void *data = malloc(total_size);
	if (data == nullptr)
	{
		WARN_LOG(SAVESTATE, "Failed to load state - could not malloc %d bytes", total_size);
		os_notify(i18n::T("Failed to load state"), 5000, i18n::T("Not enough memory"));
		if (zipFile.rawFile() == nullptr)
			std::fclose(f);
		else
			zipFile.Close();
		return;
	}

	size_t read_size;
	if (zipFile.rawFile() != nullptr)
	{
		read_size = zipFile.Read(data, total_size);
		zipFile.Close();
	}
	else
	{
		read_size = std::fread(data, 1, total_size, f);
		std::fclose(f);
	}
	if (read_size != total_size)
	{
		WARN_LOG(SAVESTATE, "Failed to load state - I/O error");
		os_notify(i18n::T("Failed to load state"), 5000, i18n::T("I/O error"));
		free(data);
		return;
	}

	try {
		Deserializer deser(data, total_size);
		emu.loadstate(deser);
	    NOTICE_LOG(SAVESTATE, "Loaded state ver %d from %s size %d", deser.version(), filename.c_str(), total_size);
		if (deser.size() != total_size)
			// Note: this isn't true for RA savestates
			WARN_LOG(SAVESTATE, "Savestate size %d but only %d bytes used", total_size, (int)deser.size());
	} catch (const Deserializer::Exception& e) {
		ERROR_LOG(SAVESTATE, "%s", e.what());
		os_notify(i18n::T("Failed to load state"), 5000, e.what());
	}

	free(data);
}

time_t dc_getStateCreationDate(int index)
{
	std::string filename = hostfs::getSavestatePath(index, false);
	if (filename != lastStateFile)
	{
		lastStateFile = filename;
		FILE *f = hostfs::storage().openFile(filename, "rb");
		if (f == nullptr)
			lastStateTime = 0;
		else
		{
			SavestateHeader header;
			if (std::fread(&header, sizeof(header), 1, f) != 1 || !header.isValid())
			{
				std::fclose(f);
				try {
					hostfs::FileInfo fileInfo = hostfs::storage().getFileInfo(filename);
					lastStateTime = fileInfo.updateTime;
				} catch (...) {
					lastStateTime = 0;
				}
			}
			else {
				std::fclose(f);
				lastStateTime = (time_t)header.creationDate;
			}
		}
	}
	return lastStateTime;
}

void dc_getStateScreenshot(int index, std::vector<u8>& pngData)
{
	pngData.clear();
	std::string filename = hostfs::getSavestatePath(index, false);
	FILE *f = hostfs::storage().openFile(filename, "rb");
	if (f == nullptr)
		return;
	SavestateHeader header;
	if (std::fread(&header, sizeof(header), 1, f) == 1 && header.isValid() && header.pngSize != 0)
	{
		pngData.resize(header.pngSize);
		if (std::fread(pngData.data(), 1, pngData.size(), f) != pngData.size())
			pngData.clear();
	}
	std::fclose(f);
}

#endif
