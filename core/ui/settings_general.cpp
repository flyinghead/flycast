/*
	Copyright 2025 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "settings.h"
#include "gui.h"
#include "IconsFontAwesome6.h"
#include "mainui.h"
#include "oslib/storage.h"
#include "stdclass.h"
#include "achievements/achievements.h"
#include "imgui_stdlib.h"
#include <array>
#include <cstring>

enum class SaveMigrationKind
{
	None,
	Vmu,
	Savestate,
	GameSave,
};

static std::vector<std::string>* g_currentPathList = nullptr;
static std::string* g_currentSinglePath = nullptr;
static SaveMigrationKind g_currentMigrationKind = SaveMigrationKind::None;

static void manageSinglePathCallback(std::string selection);
static void managePathListCallback(std::string selection);
static void handlePathSelection(SaveMigrationKind kind, std::string selection);

#ifdef __ANDROID__
struct SaveMigrationFile
{
	std::string name;
	std::string source;
	std::string destination;
};

enum class SaveMigrationPhase
{
	Idle,
	Copying,
	Verifying,
	Deleting,
};

struct SaveMigrationState
{
	SaveMigrationKind kind = SaveMigrationKind::None;
	std::string* singlePath = nullptr;
	std::vector<std::string>* pathList = nullptr;
	std::string selection;
	std::vector<SaveMigrationFile> files;
	std::vector<SaveMigrationFile> conflicts;
	hostfs::File *sourceFile = nullptr;
	hostfs::File *destinationFile = nullptr;
	SaveMigrationPhase phase = SaveMigrationPhase::Idle;
	size_t currentIndex = 0;
	s64 currentFileSize = 0;
	s64 copiedBytes = 0;
	s64 verifiedBytes = 0;
	bool moveExisting = false;
	bool overwriteConflicts = false;
	bool running = false;
	bool failed = false;
	bool openActionPopup = false;
	bool openConflictPopup = false;
	bool openProgressPopup = false;
};

static SaveMigrationState g_saveMigration;
static constexpr size_t SaveMigrationChunkSize = 128 * 1024;

static void closeSaveMigrationFiles()
{
	delete g_saveMigration.destinationFile;
	g_saveMigration.destinationFile = nullptr;
	delete g_saveMigration.sourceFile;
	g_saveMigration.sourceFile = nullptr;
}

static void resetSaveMigration()
{
	closeSaveMigrationFiles();
	g_currentSinglePath = nullptr;
	g_currentPathList = nullptr;
	g_currentMigrationKind = SaveMigrationKind::None;
	g_saveMigration = {};
}

static bool endsWith(const std::string& value, const char *suffix)
{
	size_t len = strlen(suffix);
	return value.size() >= len && value.compare(value.size() - len, len, suffix) == 0;
}

static bool isMigratableSaveFile(SaveMigrationKind kind, std::string name)
{
	string_tolower(name);
	switch (kind)
	{
	case SaveMigrationKind::Vmu:
		return name.find("vmu_save") != std::string::npos && endsWith(name, ".bin");
	case SaveMigrationKind::Savestate:
		return endsWith(name, ".state") || endsWith(name, ".state.net") || endsWith(name, ".state.tmp");
	case SaveMigrationKind::GameSave:
		return name.find("nvmem") != std::string::npos
			|| endsWith(name, ".eeprom")
			|| endsWith(name, ".card")
			|| endsWith(name, "-hopper.bin");
	default:
		return false;
	}
}

static void addUniquePath(std::vector<std::string>& paths, const std::string& path)
{
	if (!path.empty() && std::find(paths.begin(), paths.end(), path) == paths.end())
		paths.push_back(path);
}

static void collectSaveMigrationFiles(SaveMigrationKind kind, const std::vector<std::string>& sourcePaths,
		const std::string& destinationPath, std::vector<SaveMigrationFile>& files)
{
	for (const auto& sourcePath : sourcePaths)
	{
		try {
			for (const hostfs::FileInfo& info : hostfs::storage().listContent(sourcePath))
			{
				if (info.isDirectory || !isMigratableSaveFile(kind, info.name))
					continue;

				std::string destination = hostfs::storage().getSubPath(destinationPath, info.name);
				if (destination == info.path)
					continue;
				files.push_back({ info.name, info.path, destination });
			}
		} catch (const FlycastException& e) {
			WARN_LOG(COMMON, "Save migration: can't scan '%s': %s", sourcePath.c_str(), e.what());
		}
	}
}

static bool openCurrentMigrationFile()
{
	const SaveMigrationFile& file = g_saveMigration.files[g_saveMigration.currentIndex];
	g_saveMigration.currentFileSize = 0;
	g_saveMigration.copiedBytes = 0;
	g_saveMigration.verifiedBytes = 0;
	if (!g_saveMigration.moveExisting
			|| (!g_saveMigration.overwriteConflicts && hostfs::storage().exists(file.destination)))
	{
		g_saveMigration.phase = SaveMigrationPhase::Deleting;
		return true;
	}

	g_saveMigration.sourceFile = hostfs::storage().openFile(file.source, "rb");
	if (g_saveMigration.sourceFile == nullptr)
	{
		g_saveMigration.failed = true;
		g_saveMigration.currentIndex++;
		return false;
	}
	g_saveMigration.destinationFile = hostfs::storage().openFile(file.destination, "wb");
	if (g_saveMigration.destinationFile == nullptr)
	{
		closeSaveMigrationFiles();
		g_saveMigration.failed = true;
		g_saveMigration.currentIndex++;
		return false;
	}
	g_saveMigration.currentFileSize = g_saveMigration.sourceFile->size();
	g_saveMigration.phase = SaveMigrationPhase::Copying;
	return true;
}

static void failCurrentMigrationFile(bool removeDestination)
{
	const SaveMigrationFile& file = g_saveMigration.files[g_saveMigration.currentIndex];
	closeSaveMigrationFiles();
	if (removeDestination)
		hostfs::storage().remove(file.destination);
	g_saveMigration.failed = true;
	g_saveMigration.currentIndex++;
	g_saveMigration.phase = SaveMigrationPhase::Idle;
}

static void startCurrentMigrationVerification()
{
	const SaveMigrationFile& file = g_saveMigration.files[g_saveMigration.currentIndex];
	closeSaveMigrationFiles();
	g_saveMigration.sourceFile = hostfs::storage().openFile(file.source, "rb");
	g_saveMigration.destinationFile = hostfs::storage().openFile(file.destination, "rb");
	if (g_saveMigration.sourceFile == nullptr || g_saveMigration.destinationFile == nullptr
			|| g_saveMigration.sourceFile->size() != g_saveMigration.destinationFile->size())
	{
		failCurrentMigrationFile(true);
		return;
	}
	g_saveMigration.currentFileSize = g_saveMigration.sourceFile->size();
	g_saveMigration.verifiedBytes = 0;
	g_saveMigration.phase = SaveMigrationPhase::Verifying;
}

static bool processSaveMigrationStep()
{
	static std::array<u8, SaveMigrationChunkSize> sourceBuffer;
	static std::array<u8, SaveMigrationChunkSize> destinationBuffer;

	if (g_saveMigration.currentIndex >= g_saveMigration.files.size())
		return true;

	if (g_saveMigration.phase == SaveMigrationPhase::Idle && !openCurrentMigrationFile())
		return g_saveMigration.currentIndex >= g_saveMigration.files.size();

	const SaveMigrationFile& file = g_saveMigration.files[g_saveMigration.currentIndex];
	switch (g_saveMigration.phase)
	{
	case SaveMigrationPhase::Copying:
	{
		size_t read = g_saveMigration.sourceFile->read(sourceBuffer.data(), 1, sourceBuffer.size());
		if (read > 0)
		{
			if (g_saveMigration.destinationFile->write(sourceBuffer.data(), 1, read) != read)
			{
				failCurrentMigrationFile(true);
				return false;
			}
			g_saveMigration.copiedBytes += read;
			return false;
		}
		if (g_saveMigration.sourceFile->error() != 0 || g_saveMigration.destinationFile->error() != 0
				|| g_saveMigration.copiedBytes != g_saveMigration.currentFileSize)
		{
			failCurrentMigrationFile(true);
			return false;
		}
		startCurrentMigrationVerification();
		return false;
	}
	case SaveMigrationPhase::Verifying:
	{
		size_t sourceRead = g_saveMigration.sourceFile->read(sourceBuffer.data(), 1, sourceBuffer.size());
		size_t destinationRead = g_saveMigration.destinationFile->read(destinationBuffer.data(), 1, destinationBuffer.size());
		if (sourceRead != destinationRead || std::memcmp(sourceBuffer.data(), destinationBuffer.data(), sourceRead) != 0)
		{
			failCurrentMigrationFile(true);
			return false;
		}
		if (sourceRead > 0)
		{
			g_saveMigration.verifiedBytes += sourceRead;
			return false;
		}
		if (g_saveMigration.sourceFile->error() != 0 || g_saveMigration.destinationFile->error() != 0
				|| g_saveMigration.verifiedBytes != g_saveMigration.currentFileSize)
		{
			failCurrentMigrationFile(true);
			return false;
		}
		closeSaveMigrationFiles();
		g_saveMigration.phase = SaveMigrationPhase::Deleting;
		return false;
	}
	case SaveMigrationPhase::Deleting:
		if (!hostfs::storage().remove(file.source))
			g_saveMigration.failed = true;
		g_saveMigration.currentIndex++;
		g_saveMigration.phase = SaveMigrationPhase::Idle;
		return g_saveMigration.currentIndex >= g_saveMigration.files.size();
	default:
		return false;
	}
}

static void finishSaveMigrationSelection()
{
	if (g_saveMigration.singlePath != nullptr)
		*g_saveMigration.singlePath = g_saveMigration.selection;
	else if (g_saveMigration.pathList != nullptr)
	{
		g_saveMigration.pathList->erase(std::remove(g_saveMigration.pathList->begin(), g_saveMigration.pathList->end(),
				g_saveMigration.selection), g_saveMigration.pathList->end());
		if (g_saveMigration.kind == SaveMigrationKind::Savestate)
			g_saveMigration.pathList->insert(g_saveMigration.pathList->begin(), g_saveMigration.selection);
		else
			g_saveMigration.pathList->push_back(g_saveMigration.selection);
	}

	resetSaveMigration();
}

static void beginSaveMigrationRun(bool overwriteConflicts)
{
	closeSaveMigrationFiles();
	g_saveMigration.overwriteConflicts = overwriteConflicts;
	g_saveMigration.currentIndex = 0;
	g_saveMigration.currentFileSize = 0;
	g_saveMigration.copiedBytes = 0;
	g_saveMigration.verifiedBytes = 0;
	g_saveMigration.phase = SaveMigrationPhase::Idle;
	g_saveMigration.running = true;
	g_saveMigration.failed = false;
	g_saveMigration.openProgressPopup = true;
}

static void continueSaveMigration(bool moveExisting)
{
	g_saveMigration.moveExisting = moveExisting;
	if (!moveExisting)
	{
		beginSaveMigrationRun(false);
		return;
	}

	g_saveMigration.conflicts.clear();
	for (const SaveMigrationFile& file : g_saveMigration.files)
	{
		if (hostfs::storage().exists(file.destination))
			g_saveMigration.conflicts.push_back(file);
	}
	if (g_saveMigration.conflicts.empty())
		beginSaveMigrationRun(false);
	else
		g_saveMigration.openConflictPopup = true;
}

static void startSaveMigration(SaveMigrationKind kind, std::string selection)
{
	g_saveMigration.kind = kind;
	g_saveMigration.singlePath = g_currentSinglePath;
	g_saveMigration.pathList = g_currentPathList;
	g_saveMigration.selection = std::move(selection);
	g_saveMigration.files.clear();
	g_saveMigration.conflicts.clear();

	std::vector<std::string> sourcePaths;
	addUniquePath(sourcePaths, get_writable_data_path(""));
	if (kind == SaveMigrationKind::Vmu)
	{
		addUniquePath(sourcePaths, config::VMUPath.get());
		addUniquePath(sourcePaths, get_writable_config_path(""));
	}
	else if (kind == SaveMigrationKind::Savestate)
	{
		for (const auto& path : config::SavestatePath.get())
			addUniquePath(sourcePaths, path);
	}
	else if (kind == SaveMigrationKind::GameSave)
	{
		addUniquePath(sourcePaths, config::SavePath.get());
	}

	collectSaveMigrationFiles(kind, sourcePaths, g_saveMigration.selection, g_saveMigration.files);
	if (g_saveMigration.files.empty())
		finishSaveMigrationSelection();
	else
		g_saveMigration.openActionPopup = true;
}

static void handlePathSelection(SaveMigrationKind kind, std::string selection)
{
	if (kind == SaveMigrationKind::None)
	{
		if (g_currentSinglePath != nullptr)
			manageSinglePathCallback(selection);
		else
			managePathListCallback(selection);
	}
	else
		startSaveMigration(kind, std::move(selection));
}

static void centerNextSaveMigrationPopup()
{
	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
			viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
			ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

static void centerCurrentSaveMigrationPopup()
{
	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	const ImVec2 size = ImGui::GetWindowSize();
	ImGui::SetWindowPos(ImVec2(
			viewport->WorkPos.x + (viewport->WorkSize.x - size.x) * 0.5f,
			viewport->WorkPos.y + (viewport->WorkSize.y - size.y) * 0.5f));
}

static void drawSaveMigrationPopups()
{
	const char *actionPopup = T("Move existing saves?");
	if (g_saveMigration.openActionPopup)
	{
		ImGui::OpenPopup(actionPopup);
		g_saveMigration.openActionPopup = false;
	}
	centerNextSaveMigrationPopup();
	if (ImGui::BeginPopupModal(actionPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		centerCurrentSaveMigrationPopup();
		ImGui::TextWrapped("%s", T("Move existing saves to the selected folder? Choosing Delete removes the originals instead."));
		ImGui::Spacing();
		if (ImGui::Button(T("Move"), ScaledVec2(100.f, 0)))
		{
			ImGui::CloseCurrentPopup();
			continueSaveMigration(true);
		}
		ImGui::SameLine();
		if (ImGui::Button(T("Delete"), ScaledVec2(100.f, 0)))
		{
			ImGui::CloseCurrentPopup();
			continueSaveMigration(false);
		}
		ImGui::SameLine();
		if (ImGui::Button(T("Cancel"), ScaledVec2(100.f, 0)))
		{
			resetSaveMigration();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	const char *conflictPopup = T("Save files already exist");
	if (g_saveMigration.openConflictPopup)
	{
		ImGui::OpenPopup(conflictPopup);
		g_saveMigration.openConflictPopup = false;
	}
	centerNextSaveMigrationPopup();
	if (ImGui::BeginPopupModal(conflictPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		centerCurrentSaveMigrationPopup();
		ImGui::TextWrapped("%s", T("The selected folder already contains save files with the same names. Skip keeps those files and deletes the old originals."));
		ImGui::Spacing();
		if (ImGui::Button(T("Overwrite"), ScaledVec2(120.f, 0)))
		{
			ImGui::CloseCurrentPopup();
			beginSaveMigrationRun(true);
		}
		ImGui::SameLine();
		if (ImGui::Button(T("Skip"), ScaledVec2(100.f, 0)))
		{
			ImGui::CloseCurrentPopup();
			beginSaveMigrationRun(false);
		}
		ImGui::SameLine();
		if (ImGui::Button(T("Cancel"), ScaledVec2(100.f, 0)))
		{
			resetSaveMigration();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	const char *progressPopup = T("Migrating saves");
	if (g_saveMigration.openProgressPopup)
	{
		ImGui::OpenPopup(progressPopup);
		g_saveMigration.openProgressPopup = false;
	}
	centerNextSaveMigrationPopup();
	if (ImGui::BeginPopupModal(progressPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		centerCurrentSaveMigrationPopup();
		size_t totalFiles = g_saveMigration.files.size();
		size_t currentFileIndex = std::min(g_saveMigration.currentIndex, totalFiles);
		float fileProgress = 0.f;
		if (g_saveMigration.currentIndex < totalFiles && g_saveMigration.currentFileSize > 0)
		{
			if (g_saveMigration.phase == SaveMigrationPhase::Copying)
				fileProgress = std::min(1.f, (float)g_saveMigration.copiedBytes / g_saveMigration.currentFileSize);
			else if (g_saveMigration.phase == SaveMigrationPhase::Verifying)
				fileProgress = std::min(1.f, (float)g_saveMigration.verifiedBytes / g_saveMigration.currentFileSize);
			else if (g_saveMigration.phase == SaveMigrationPhase::Deleting)
				fileProgress = 1.f;
		}
		float totalProgress = totalFiles == 0 ? 1.f : (currentFileIndex + fileProgress) / totalFiles;
		ImGui::Text("%s", T("Migrating save files..."));
		if (g_saveMigration.currentIndex < totalFiles)
			ImGui::TextWrapped("%s", g_saveMigration.files[g_saveMigration.currentIndex].name.c_str());
		ImGui::ProgressBar(totalProgress, ScaledVec2(320.f, 0));
		ImGui::Text("%zu / %zu", currentFileIndex, totalFiles);

		if (processSaveMigrationStep())
		{
			bool failed = g_saveMigration.failed;
			ImGui::CloseCurrentPopup();
			if (failed)
				gui_error(Ts("Some save files could not be migrated or deleted."));
			finishSaveMigrationSelection();
		}
		ImGui::EndPopup();
	}
}
#else
static void handlePathSelection(SaveMigrationKind kind, std::string selection)
{
	(void)kind;
	if (g_currentSinglePath != nullptr)
		manageSinglePathCallback(std::move(selection));
	else
		managePathListCallback(std::move(selection));
}
#endif

static void manageSinglePathCallback(std::string selection)
{
    if (g_currentSinglePath != nullptr)
    {
        *g_currentSinglePath = selection;
        g_currentSinglePath = nullptr;
        g_currentMigrationKind = SaveMigrationKind::None;
    }
}

static void managePathListCallback(std::string selection)
{
    if (g_currentPathList != nullptr)
    {
        g_currentPathList->push_back(selection);
        g_currentPathList = nullptr;
        g_currentMigrationKind = SaveMigrationKind::None;
    }
}

static void manageSinglePath(const char* label, const char *popupName, config::Option<std::string, false>& pathOption, const char* helpText,
		bool writeAccess = false, SaveMigrationKind migrationKind = SaveMigrationKind::None)
{
    ImVec2 size;
    size.x = 0.0f;
    size.y = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f;
    bool openPopup = false;
    
    ImVec2 childSize;
    if (beginFrame(label, size, &childSize))
    {
        ImGui::AlignTextToFramePadding();
        if (pathOption.get().empty()) {
            ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(24, 3));
            std::string buttonLabel = T("Add") + std::string("##") + label;
            openPopup = ImGui::Button(buttonLabel.c_str());
        }
        else
        {
            float w = childSize.x - ImGui::CalcTextSize(ICON_FA_TRASH_CAN).x - ImGui::GetStyle().FramePadding.x * 2
                - ImGui::GetStyle().ItemSpacing.x;
            std::string s = middleEllipsis(pathOption, w);
            ImGui::Text("%s", s.c_str());
            ImGui::SameLine(0, w - ImGui::CalcTextSize(s.c_str()).x + ImGui::GetStyle().ItemSpacing.x);
            std::string buttonLabel = std::string(ICON_FA_TRASH_CAN "##") + label;
            if (ImGui::Button(buttonLabel.c_str()))
                pathOption.get().clear();
        }
        endFrame();
    }
    ImGui::SameLine();
    ShowHelpMarker(helpText);
    
    select_file_popup(popupName, [](bool cancelled, std::string selection) {
		if (!cancelled)
			handlePathSelection(g_currentMigrationKind, std::move(selection));
		else
		{
			g_currentSinglePath = nullptr;
			g_currentMigrationKind = SaveMigrationKind::None;
		}
    	return true;
    });
    if (openPopup)
    {
        g_currentSinglePath = &pathOption.get();
        g_currentMigrationKind = migrationKind;
#ifdef __ANDROID__
		bool supported = hostfs::addStorage(true, writeAccess, T(popupName), [](bool cancelled, std::string selection) {
			if (!cancelled)
				handlePathSelection(g_currentMigrationKind, std::move(selection));
			else
			{
				g_currentSinglePath = nullptr;
				g_currentMigrationKind = SaveMigrationKind::None;
			}
		});
		if (!supported)
			ImGui::OpenPopup(T(popupName));
#else
        ImGui::OpenPopup(T(popupName));
#endif
    }
}

static void managePathList(const char* label, const char *popupName, std::vector<std::string>& paths, const char* helpText,
		bool writeAccess = false, SaveMigrationKind migrationKind = SaveMigrationKind::None)
{
    ImguiID _(label);
    ImVec2 size;
    size.x = 0.0f;
    size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f)
                * (paths.size() + 1);

    bool openPopup = false;
    ImVec2 childSize;
    if (beginFrame(label, size, &childSize))
    {
        ImGui::AlignTextToFramePadding();
        int to_delete = -1;
        for (u32 i = 0; i < paths.size(); i++)
        {
            ImguiID _(std::to_string(i).c_str());
            float maxW = childSize.x - ImGui::CalcTextSize(ICON_FA_TRASH_CAN).x - ImGui::GetStyle().FramePadding.x * 2
                         - ImGui::GetStyle().ItemSpacing.x;
            std::string s = middleEllipsis(paths[i], maxW);
            ImGui::Text("%s", s.c_str());
            ImGui::SameLine(0, maxW - ImGui::CalcTextSize(s.c_str()).x + ImGui::GetStyle().ItemSpacing.x);
            if (ImGui::Button(ICON_FA_TRASH_CAN))
                to_delete = (int)i;
        }

        ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(24, 3));
        std::string buttonLabel = T("Add") + std::string("##") + label;
        openPopup = ImGui::Button(buttonLabel.c_str());

        endFrame();
        if (to_delete >= 0)
        {
            paths.erase(paths.begin() + to_delete);
            SaveSettings();
        }
    }
    ImGui::SameLine();
    ShowHelpMarker(helpText);

    // Handle file selection popup (following the same pattern as addContentPath)
    if (openPopup)
    {
	    g_currentPathList = &paths;
	    g_currentMigrationKind = migrationKind;
    }
    select_file_popup(popupName, [](bool cancelled, std::string selection) {
		if (!cancelled)
			handlePathSelection(g_currentMigrationKind, std::move(selection));
		else
		{
			g_currentPathList = nullptr;
			g_currentMigrationKind = SaveMigrationKind::None;
		}
    	return true;
    });
#ifdef __ANDROID__
    if (openPopup)
    {
		bool supported = hostfs::addStorage(true, writeAccess, T(popupName), [](bool cancelled, std::string selection) {
			if (!cancelled)
				handlePathSelection(g_currentMigrationKind, std::move(selection));
			else
			{
				g_currentPathList = nullptr;
				g_currentMigrationKind = SaveMigrationKind::None;
			}
		});
		if (!supported)
			ImGui::OpenPopup(T(popupName));
    }
#else
    if (openPopup)
        ImGui::OpenPopup(T(popupName));
#endif
}

static void addContentPathCallback(const std::string& path)
{
	auto& contentPath = config::ContentPath.get();
	if (std::count(contentPath.begin(), contentPath.end(), path) == 0)
	{
		scanner.stop();
		contentPath.push_back(path);
		if (gui_state == GuiState::Main)
			// when adding content path from empty game list
			SaveSettings();
		scanner.refresh();
	}
}

void addContentPath(bool start)
{
    const char *title = T("Select a Content Folder");
    select_file_popup(title, [](bool cancelled, std::string selection) {
		if (!cancelled)
			addContentPathCallback(selection);
		return true;
    });
#ifdef __ANDROID__
    if (start)
    {
    	bool supported = hostfs::addStorage(true, false, title, [](bool cancelled, std::string selection) {
    		if (!cancelled)
    			addContentPathCallback(selection);
    	});
    	if (!supported)
    		ImGui::OpenPopup(title);
    }
#else
    if (start)
    	ImGui::OpenPopup(title);
#endif
}

void gui_settings_general()
{
#ifdef __ANDROID__
	drawSaveMigrationPopups();
#endif
	struct
	{
		const char* label;
		const char* value;
	}
	UILanguages[] = {
		{ T("System Default"), "" },
		{ "English", "en" },
		{ "Français", "fr" },
		{ "Magyar", "hu" },
		{ "日本語", "ja" },
		{ "Português (Brasil)", "pt_BR" },
		{ "Svenska", "sv" },
	};

	// Determine the preview text
	const std::string currentLanguage = config::UILanguage.get();
	std::string preview;
	for (const auto& it : UILanguages)
	{
		if (currentLanguage == it.value) {
			preview = it.label;
			break;
		}
	}

	if (ImGui::BeginCombo(T("UI Language"), preview.c_str()))
	{
		for (const auto& lang : UILanguages)
		{
			const bool selected = (currentLanguage == lang.value);

			if (ImGui::Selectable(lang.label, selected)) {
				config::UILanguage = lang.value;
				i18n::reloadLanguage();
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	{
		DisabledScope scope(settings.platform.isArcade());

		const char *languages[] = { T("Japanese"), T("English"), T("German"), T("French"), T("Spanish"), T("Italian"), T("Default") };
		OptionComboBox(T("Dreamcast Language"), config::Language, languages, std::size(languages),
				T("The language as configured in the Dreamcast BIOS"));

		const char *broadcast[] = { "NTSC", "PAL", "PAL/M", "PAL/N", T("Default") };
		OptionComboBox(T("Broadcast"), config::Broadcast, broadcast, std::size(broadcast),
				T("TV broadcasting standard for non-VGA modes"));
	}

	const char *consoleRegion[] = { T("Japan"), T("USA"), T("Europe"), T("Default") };
	const char *arcadeRegion[] = { T("Japan"), T("USA"), i18n::translateCtx("region", "Export"), T("Korea") };
	const char **region = settings.platform.isArcade() ? arcadeRegion : consoleRegion;
	OptionComboBox(T("Region"), config::Region, region, std::size(consoleRegion),
			T("BIOS region"));

	const char *cable[] = { T("VGA"), T("RGB Component"), T("TV Composite") };
	{
		DisabledScope scope(config::Cable.isReadOnly() || settings.platform.isArcade());

		const char *value = config::Cable == 0 ? cable[0]
				: config::Cable > 0 && config::Cable <= (int)std::size(cable) ? cable[config::Cable - 1]
				: "?";
		if (ImGui::BeginCombo(T("Cable"), value, ImGuiComboFlags_None))
		{
			for (int i = 0; i < IM_ARRAYSIZE(cable); i++)
			{
				bool is_selected = i == 0 ? config::Cable <= 1 : config::Cable - 1 == i;
				if (ImGui::Selectable(cable[i], &is_selected))
					config::Cable = i == 0 ? 0 : i + 1;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
        ImGui::SameLine();
        ShowHelpMarker(T("Video connection type"));
	}

#if !defined(TARGET_IPHONE)
    ImVec2 size;
    size.x = 0.0f;
    size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f)
    				* (config::ContentPath.get().size() + 1);

    ImVec2 childSize;
    if (beginFrame(T("Content Location"), size, &childSize))
    {
    	int to_delete = -1;
        for (u32 i = 0; i < config::ContentPath.get().size(); i++)
        {
        	ImguiID _(config::ContentPath.get()[i].c_str());
            ImGui::AlignTextToFramePadding();
            float maxW = childSize.x - ImGui::CalcTextSize(ICON_FA_TRASH_CAN).x - ImGui::GetStyle().FramePadding.x * 2
            		 - ImGui::GetStyle().ItemSpacing.x;
            std::string s = middleEllipsis(config::ContentPath.get()[i], maxW);
        	ImGui::Text("%s", s.c_str());
        	ImGui::SameLine(0, maxW - ImGui::CalcTextSize(s.c_str()).x + ImGui::GetStyle().ItemSpacing.x);
        	if (ImGui::Button(ICON_FA_TRASH_CAN))
        		to_delete = i;
        }

        ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(24, 3));
        const bool addContent = ImGui::Button((T("Add") + std::string("##") + "ContentLocation").c_str());
        addContentPath(addContent);
        ImGui::SameLine();

        if (ImGui::Button(T("Rescan Content")))
			scanner.refresh();

		endFrame();
    	if (to_delete >= 0)
    	{
    		scanner.stop();
    		config::ContentPath.get().erase(config::ContentPath.get().begin() + to_delete);
			scanner.refresh();
    	}
    }
    ImGui::SameLine();
    ShowHelpMarker(T("The folders where your games are stored"));

    size.y = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImGui::Spacing();

#if defined(__linux__) && !defined(__ANDROID__)
    if (beginFrame(T("Data Folder"), size, &childSize))
    {
    	float w = childSize.x - ImGui::GetStyle().FramePadding.x;
    	std::string s = middleEllipsis(get_writable_data_path(""), w);
        ImGui::Text("%s", s.c_str());
        endFrame();
    }
    ImGui::SameLine();
    ShowHelpMarker(T("The folder containing BIOS files, as well as saved VMUs and states"));
#else
#if defined(__ANDROID__) || defined(TARGET_MAC)
    size.y += ImGui::GetTextLineHeightWithSpacing();
#endif
    if (beginFrame(T("Home Folder"), size, &childSize))
    {
    	float w = childSize.x - ImGui::GetStyle().FramePadding.x;
    	std::string s = middleEllipsis(get_writable_config_path(""), w);
        ImGui::Text("%s", s.c_str());
        ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(24, 3));
#ifdef __ANDROID__
        {
        	DisabledScope _(!config::UseSafFilePicker);
			if (ImGui::Button(i18n::translateCtx("action", "Import")))
				hostfs::importHomeDirectory();
			ImGui::SameLine();
			if (ImGui::Button(i18n::translateCtx("action", "Export")))
				hostfs::exportHomeDirectory();
        }
#endif
#ifdef TARGET_MAC
        if (ImGui::Button(T("Reveal in Finder")))
        {
            char temp[512];
            snprintf(temp, sizeof(temp), "open \"%s\"", get_writable_config_path("").c_str());
            system(temp);
        }
#endif
        endFrame();
    }
    ImGui::SameLine();
    ShowHelpMarker(T("The folder where Flycast saves configuration files and VMUs. BIOS files should be in a subfolder named \"data\""));
#endif // !linux
    ImGui::Spacing();
#else // TARGET_IPHONE
    {
    	ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(24, 3));
		if (ImGui::Button(T("Rescan Content")))
			scanner.refresh();
    }
#endif
    ImGui::Spacing();
	OptionCheckbox(T("Box Art Game List"), config::BoxartDisplayMode,
			T("Display game cover art in the game list."));
	OptionCheckbox(T("Fetch Box Art"), config::FetchBoxart,
			T("Fetch cover images from TheGamesDB.net."));
	if (OptionSlider(T("UI Scaling"), config::UIScaling, 50, 200, T("Adjust the size of UI elements and fonts."), "%d%%"))
		uiUserScaleUpdated = true;
	if (uiUserScaleUpdated)
	{
		ImGui::SameLine();
		if (ImGui::Button(T("Apply"))) {
			mainui_reinit();
			uiUserScaleUpdated = false;
		}
	}

	const char *themes[] = { T("Dark"), T("Light"), T("Dreamcast"), T("High Contrast"), T("Nintendo"), T("Aqua Chill") };
	int previousUITheme = config::UITheme;
	OptionComboBox(T("UI Theme"), config::UITheme, themes, std::size(themes),
			T("Select the UI color theme."));
	// Auto-apply theme when selection changes
	if (previousUITheme != config::UITheme) {
		applyCurrentTheme();
	}

	if (OptionCheckbox(T("Hide Legacy Naomi Roms"), config::HideLegacyNaomiRoms,
			T("Hide .bin, .dat and .lst files from the content browser")))
		scanner.refresh();
#ifdef __ANDROID__
	OptionCheckbox(T("Use SAF File Picker"), config::UseSafFilePicker,
			T("Use Android Storage Access Framework file picker to select folders and files. Ignored on Android 10 and later."));
#endif

	ImGui::Text("%s", T("Automatic State:"));
	OptionCheckbox(T("Load"), config::AutoLoadState,
			T("Load the last saved state of the game when starting"));
	ImGui::SameLine();
	OptionCheckbox(T("Save"), config::AutoSaveState,
			T("Save the state of the game when stopping"));
	OptionCheckbox(T("Naomi Free Play"), config::ForceFreePlay, T("Configure Naomi games in Free Play mode."));
#if USE_DISCORD
	OptionCheckbox(T("Discord Presence"), config::DiscordPresence, T("Show which game you are playing on Discord"));
#endif
#ifdef USE_RACHIEVEMENTS
	OptionCheckbox(T("Enable RetroAchievements"), config::EnableAchievements, T("Track your game achievements using RetroAchievements.org"));
	{
		DisabledScope _(!config::EnableAchievements);
		ImGui::Indent();
		OptionCheckbox(T("Hardcore Mode"), config::AchievementsHardcoreMode,
				T("Enable RetroAchievements hardcore mode. Using cheats and loading a state are not allowed in this mode."));
		InputText(T("Username"), &config::AchievementsUserName.get(),
				achievements::isLoggedOn() ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);
		if (config::EnableAchievements)
		{
			static std::future<void> futureLogin;
			achievements::init();
			if (achievements::isLoggedOn())
			{
				ImGui::Text("%s", T("Authentication successful"));
				if (futureLogin.valid())
					futureLogin.get();
				if (ImGui::Button(T("Logout"), ScaledVec2(100, 0)))
					achievements::logout();
			}
			else
			{
				static char password[256];
				InputText(T("Password"), password, sizeof(password), ImGuiInputTextFlags_Password);
				if (futureLogin.valid())
				{
					if (futureLogin.wait_for(std::chrono::seconds::zero()) == std::future_status::timeout) {
						ImGui::Text("%s", T("Authenticating..."));
					}
					else
					{
						try {
							futureLogin.get();
						} catch (const FlycastException& e) {
							gui_error(e.what());
						}
					}
				}
				{
					DisabledScope _(config::AchievementsUserName.get().empty() || password[0] == '\0');
					if (ImGui::Button(T("Login"), ScaledVec2(100, 0)) && !futureLogin.valid())
					{
						futureLogin = achievements::login(config::AchievementsUserName.get().c_str(), password);
						memset(password, 0, sizeof(password));
					}
				}
			}
		}
		ImGui::Unindent();
	}
#endif

// Custom Paths section - hidden on Android and iOS
#if !defined(TARGET_IPHONE)
    ImGui::Spacing();
    header(T("Custom Paths"));

    managePathList(T("BIOS Folders"), T("Select a BIOS folder"), config::BiosPath.get(),
    		T("Folders containing BIOS files (e.g. dc_boot.bin or dc_bios.bin) and arcade BIOS"));
    ImGui::Spacing();

    manageSinglePath(T("VMU Folder"), T("Select the VMU folder"), config::VMUPath,
			T("Folder where VMU (.bin) saves are stored. This can be the same folder as save states and game saves"),
			true, SaveMigrationKind::Vmu);
    ImGui::Spacing();

    managePathList(T("Savestate Folders"), T("Select a savestate folder"), config::SavestatePath.get(),
			T("Folders for save states. First path is used for new states; all are searched when loading. This can be the same folder as VMUs and game saves"),
			true, SaveMigrationKind::Savestate);
    ImGui::Spacing();

    manageSinglePath(T("Game Save Folder"), T("Select the game save folder"), config::SavePath,
			T("Folder for Dreamcast internal flash and arcade NVRAM/card data. This can be the same folder as VMUs and save states"),
			true, SaveMigrationKind::GameSave);
    ImGui::Spacing();

    managePathList(T("Texture Pack Folders"), T("Select a texture pack folder"), config::TexturePath.get(),
    		T("Folders containing textures/<gameId> or <gameId> under a textures subfolder"));
    ImGui::Spacing();

#if !defined(__ANDROID__)
    manageSinglePath(T("Texture Dump Folder"), T("Select the texture dump folder"), config::TextureDumpPath,
    		T("Folder where texture dumps are saved. Game-specific subfolders will be created automatically"));
    ImGui::Spacing();
    
    manageSinglePath(T("Box Art Folder"), T("Select the box art folder"), config::BoxartPath,
    		T("Folder containing box art images (png/jpg). If empty, Flycast will use the default Home Folder/boxart for downloads and generated art"));
    ImGui::Spacing();

    managePathList(T("Controller Mapping Folders"), T("Select a controller mapping folder"), config::MappingsPath.get(),
    		T("Folders containing controller mapping files (.cfg). The emulator also looks in Home Folder/mappings. Per-game mappings are suffixed with _<gameId>.cfg"));
    ImGui::Spacing();

    managePathList(T("Cheat Folders"), T("Select a cheat folder"), config::CheatPath.get(),
    		T("Folders containing cheat files (.cht/.txt) named with the game ID. Flycast will auto-load matching files if present"));
    ImGui::Spacing();
#endif  // !ANDROID
#endif  // !IPHONE
}

static void applyDarkTheme()
{
	// Reset style first, then apply dark colors - exactly like original Flycast
	ImGui::GetStyle() = ImGuiStyle{};
	ImGui::StyleColorsDark();

	// Apply original Flycast styling to match exactly how it was
	ImGuiStyle& style = ImGui::GetStyle();
	style.TabRounding = 5.0f;
	style.FrameRounding = 3.0f;
	style.ItemSpacing = ImVec2(8, 8);		// from 8,4
	style.ItemInnerSpacing = ImVec2(4, 6);	// from 4,4

	// Reset style properties to defaults to ensure clean theme switching
	style.TabBorderSize = 0.0f;        // Revert to default
	style.FrameBorderSize = 0.0f;      // Revert to default
}

static void applyLightTheme()
{
	ImGui::StyleColorsLight();
	ImGuiStyle& style = ImGui::GetStyle();

	// Improved light theme with better contrast
	style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);           // Black text
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);   // Darker gray for disabled text

	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.97f, 1.00f);       // Very light blue-gray
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.98f, 0.98f, 1.00f, 1.00f);        // Almost white for popups (fully opaque)

	style.Colors[ImGuiCol_Border] = ImVec4(0.70f, 0.70f, 0.80f, 0.50f);         // Medium blue-gray borders (semi-transparent ok)
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);   // No border shadows

	// Darker frame backgrounds (unchecked boxes, etc.)
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.70f, 0.70f, 0.80f, 1.00f);        // Darker blue-gray backgrounds
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.60f, 0.60f, 0.75f, 1.00f); // Slightly darker when hovered
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.50f, 0.50f, 0.70f, 1.00f);  // Even darker when active

	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.70f, 0.70f, 0.85f, 1.00f);        // Light blue-gray title bar
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.60f, 0.60f, 0.80f, 1.00f);  // Darker title bar when active
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.80f, 0.80f, 0.90f, 0.75f); // Lighter when collapsed (transparency ok)

	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.80f, 0.80f, 0.90f, 1.00f);      // Light blue-gray menu bar

	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.85f, 0.85f, 0.90f, 1.00f);    // Very light scrollbar background
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.65f, 0.65f, 0.80f, 1.00f);  // Medium blue-gray scrollbar (fully opaque)
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.55f, 0.55f, 0.75f, 1.00f); // Darker when hovered (fully opaque)
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.70f, 1.00f);  // Even darker when active (fully opaque)

	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.45f, 0.90f, 1.00f);      // Bright blue checkmarks

	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.50f, 0.80f, 1.00f);     // Medium blue-gray slider (fully opaque)
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.40f, 0.70f, 1.00f); // Darker when active (fully opaque)

	style.Colors[ImGuiCol_Button] = ImVec4(0.67f, 0.67f, 0.83f, 1.00f);         // Medium blue-gray buttons (fully opaque)
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.57f, 0.57f, 0.77f, 1.00f);  // Darker when hovered (fully opaque)
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.47f, 0.47f, 0.73f, 1.00f);   // Even darker when active (fully opaque)

	style.Colors[ImGuiCol_Header] = ImVec4(0.60f, 0.60f, 0.80f, 0.80f);         // Medium blue-gray headers (semi-transparent ok)
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.50f, 0.50f, 0.75f, 1.00f);  // Darker when hovered (fully opaque)
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.45f, 0.45f, 0.70f, 1.00f);   // Even darker when active (fully opaque)

	style.Colors[ImGuiCol_Separator] = ImVec4(0.60f, 0.60f, 0.70f, 0.60f);      // Visible separators (semi-transparent ok)

	style.Colors[ImGuiCol_Tab] = ImVec4(0.65f, 0.65f, 0.80f, 1.00f);            // Medium blue-gray tabs (fully opaque)
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.55f, 0.55f, 0.75f, 1.00f);     // Darker when hovered (fully opaque)
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.50f, 0.50f, 0.70f, 1.00f);      // Even darker when active (fully opaque)
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.75f, 0.75f, 0.85f, 1.00f);   // Lighter when unfocused (fully opaque)
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.65f, 0.65f, 0.80f, 1.00f); // Medium when unfocused but active (fully opaque)

	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.45f, 0.45f, 0.85f, 0.35f); // Visible selection background (transparency ok)

	style.TabBorderSize = 0.0f;        // Revert to default
	style.FrameBorderSize = 0.0f;      // Revert to default
}

static void applyDreamcastTheme()
{
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();

	// Dreamcast-inspired theme with higher contrast
	// Pure white text - maximum brightness
	style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);           // Pure white text
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.70f, 0.70f, 0.70f, 0.50f);   // More transparent for disabled text (transparency ok)

	// Darker backgrounds for higher contrast
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.07f, 0.10f, 1.00f);      // Much darker DC Menu Background for contrast
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.05f, 0.07f, 0.10f, 1.00f);       // Match window background
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.16f, 0.40f, 1.00f);      // DC Logo Blue (fully opaque)
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.22f, 0.50f, 1.00f);  // Brighter DC Blue when active
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.43f, 0.73f, 0.70f); // More transparent when collapsed (transparency ok)
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);     // Dark but slightly lighter than window
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.07f, 0.10f, 1.00f);   // Darker, same as window background
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f); // DC Shell White (fully opaque)
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f); // Fully opaque when hovered
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f); // Fully opaque when active

	// Controller button colors - more vibrant
	style.Colors[ImGuiCol_Button] = ImVec4(0.90f, 0.50f, 0.10f, 1.00f);        // Brighter orange buttons
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.70f, 0.40f, 1.00f); // Brighter when hovered
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.95f, 0.45f, 0.00f, 1.00f);  // Brighter when active

	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.07f, 0.10f, 1.00f);       // Darker DC Menu Background (100% opaque)
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.17f, 0.22f, 1.00f);       // Darker DC Menu (fully opaque)
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.70f, 0.50f, 1.00f); // Brighter Y Button (Green) (fully opaque)
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.06f, 0.60f, 0.35f, 1.00f);  // Y Button (Green) (fully opaque)

	style.Colors[ImGuiCol_Header] = ImVec4(0.15f, 0.18f, 0.45f, 1.00f);        // Brighter B Button (Blue) (fully opaque)
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.23f, 0.55f, 1.00f); // Even brighter B Button (Blue) (fully opaque)
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.10f, 0.13f, 0.33f, 1.00f);  // B Button darker

	// A Button Red - more vibrant
	style.Colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.30f, 0.30f, 1.00f);     // Brighter A Button (Red) - fully opaque
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 0.30f, 0.30f, 1.00f);    // Brighter A Button (Red) (fully opaque)
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.20f, 0.20f, 1.00f); // A Button darker

	// Tabs - using X Button Blue - more vibrant
	style.Colors[ImGuiCol_Tab] = ImVec4(0.13f, 0.16f, 0.40f, 1.00f);           // X Button Blue
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.30f, 0.60f, 1.00f);    // Brighter when hovered (fully opaque)
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.30f, 0.35f, 0.65f, 1.00f);     // Much brighter & whiter when active
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.16f, 0.40f, 1.00f);  // Same blue but fully opaque
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.25f, 0.30f, 0.60f, 1.00f); // Brighter for unfocused active (fully opaque)

	// Other elements - more vibrant
	style.Colors[ImGuiCol_Border] = ImVec4(1.00f, 0.85f, 0.25f, 0.70f);         // Brighter DC "X" yellow borders (some transparency ok)
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);   // No border shadows
	style.Colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.50f, 0.80f, 0.75f);      // Brighter DC Logo Blue separators (some transparency ok)
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.50f, 0.00f, 0.35f); // DC Swirl Orange selection (transparency ok)

	// Table colors - more vibrant
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.15f, 0.18f, 0.45f, 1.00f);  // Brighter X Button Blue for headers (fully opaque)
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.50f, 0.80f, 1.00f); // Brighter DC Logo Blue borders
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.50f, 0.80f, 0.70f);  // Brighter borders (some transparency ok)
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.05f, 0.07f, 0.10f, 1.00f);     // Same as window bg
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);  // Slightly lighter for alt rows
}

static void applyHighContrastTheme()
{
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();

	// High contrast theme with Dreamcast color accents
	// Base colors - extreme contrast
	style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);           // Pure white text
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);   // Light gray for disabled text

	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);       // Pure black background
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);        // Very dark gray child windows
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);        // Very dark for popups

	style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.43f, 0.73f, 0.50f);         // DC Logo Blue borders (semi-transparent ok)
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);   // No shadows

	// Frame elements (checkboxes, input fields)
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);        // Dark gray frame backgrounds
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f); // Medium gray when hovered
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);  // Darker gray when active

	// Title bars
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);        // Dark gray title bar
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.43f, 0.73f, 1.00f);  // DC Logo Blue active title
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.43f, 0.73f, 0.50f); // Semi-transparent when collapsed (transparency ok)

	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);      // Dark gray menu bar

	// Scrollbars
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);    // Dark scrollbar background
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);  // Medium gray scrollbar
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.56f, 1.00f); // Lighter when hovered
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 0.65f, 0.90f, 1.00f);  // DC Highlight Blue when active

	// Interactive elements
	style.Colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.50f, 0.00f, 1.00f);      // DC Swirl Orange for checkmarks

	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.65f, 0.90f, 1.00f);     // DC Highlight Blue for sliders
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.50f, 0.00f, 1.00f); // DC Swirl Orange when active

	// Buttons
	style.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.43f, 0.73f, 1.00f);         // DC Logo Blue buttons
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.65f, 0.90f, 1.00f);  // DC Highlight Blue when hovered
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.33f, 0.60f, 1.00f);   // Darker blue when active

	// Headers (collapsing headers, tree nodes)
	style.Colors[ImGuiCol_Header] = ImVec4(0.00f, 0.43f, 0.73f, 0.80f);         // Semi-transparent DC Logo Blue (some transparency ok)
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.65f, 0.90f, 1.00f);  // DC Highlight Blue when hovered (fully opaque)
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.43f, 0.73f, 1.00f);   // Solid DC Logo Blue when active

	// Tables
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);  // Dark gray table headers
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.43f, 0.73f, 1.00f); // DC Logo Blue table borders
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.43f, 0.73f, 0.50f);  // Lighter DC Logo Blue inner borders (semi-transparent ok)

	// Tabs
	style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);            // Dark gray tabs
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.00f, 0.65f, 0.90f, 1.00f);     // DC Highlight Blue when hovered
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.00f, 0.43f, 0.73f, 1.00f);      // DC Logo Blue when active
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);   // Dark gray when unfocused
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.00f, 0.43f, 0.73f, 1.00f); // DC Logo Blue when unfocused but active (fully opaque)

	// Other UI elements
	style.Colors[ImGuiCol_Separator] = ImVec4(1.00f, 1.00f, 1.00f, 0.40f);      // Semi-transparent white separators (transparency ok)
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.50f, 0.00f, 0.35f); // Semi-transparent DC Swirl Orange selection (transparency ok)

	// Increase contrast even more
	style.Alpha = 1.0f;                // No transparency
	style.FrameBorderSize = 1.0f;      // Add borders to frames
	style.WindowBorderSize = 1.0f;     // Add borders to windows
	style.PopupBorderSize = 1.0f;      // Add borders to popups
	style.TabBorderSize = 1.0f;        // Add borders to tabs
}

static void applyNintendoTheme()
{
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();

	// Nintendo color palette
	ImVec4 nintendoRed = ImVec4(0.90f, 0.10f, 0.10f, 1.00f);            // Nintendo logo/Switch red
	ImVec4 nintendoRedLight = ImVec4(1.00f, 0.30f, 0.30f, 1.00f);       // Lighter red
	ImVec4 nintendoRedDark = ImVec4(0.65f, 0.05f, 0.05f, 1.00f);        // Darker red

	ImVec4 luigiGreen = ImVec4(0.00f, 0.65f, 0.00f, 1.00f);             // Luigi green
	ImVec4 luigiGreenLight = ImVec4(0.30f, 0.85f, 0.30f, 1.00f);        // Lighter green

	ImVec4 gameboy = ImVec4(0.70f, 0.80f, 0.15f, 1.00f);                // GameBoy screen color
	ImVec4 gamecubePurple = ImVec4(0.35f, 0.20f, 0.65f, 1.00f);         // GameCube purple

	// Text colors
	style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);           // Pure white text
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.70f, 0.70f, 0.70f, 0.65f);   // Light gray for disabled (keep transparency)

	// Window background and elements - darker blue-black like classic consoles
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.03f, 0.03f, 0.10f, 1.00f);       // Dark blue-black background (fully opaque)
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.03f, 0.03f, 0.10f, 1.00f);        // Match window background (fully opaque)
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.02f, 0.02f, 0.08f, 1.00f);        // Slightly darker for popup (fully opaque)

	// Frame elements - using GameCube purple for frames
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.08f, 0.20f, 1.00f);        // Dark purple-ish background (fully opaque)
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.14f, 0.35f, 1.00f); // Lighter purple when hovered (fully opaque)
	style.Colors[ImGuiCol_FrameBgActive] = gamecubePurple;                      // GameCube purple when active

	// Title elements - using Nintendo red
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.12f, 1.00f);        // Dark blue background (fully opaque)
	style.Colors[ImGuiCol_TitleBgActive] = nintendoRed;                         // Nintendo red for active title
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.05f, 0.12f, 0.75f); // Keep semi-transparent when collapsed
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.15f, 1.00f);      // Dark menu bar (fully opaque)

	// Scrollbars - GameBoy inspired
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.04f, 0.10f, 1.00f);    // Dark scrollbar background (fully opaque)
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.40f, 0.10f, 1.00f);  // GameBoy green-yellow (fully opaque)
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.50f, 0.60f, 0.15f, 1.00f); // Brighter GameBoy color (fully opaque)
	style.Colors[ImGuiCol_ScrollbarGrabActive] = gameboy;                       // Full GameBoy color when active

	// Button elements - Nintendo red
	style.Colors[ImGuiCol_Button] = nintendoRed;                                // Nintendo red button
	style.Colors[ImGuiCol_ButtonHovered] = nintendoRedLight;                    // Lighter red when hovered
	style.Colors[ImGuiCol_ButtonActive] = nintendoRedDark;                      // Darker red when active

	// Interactive elements - Luigi green for checkmarks and sliders
	style.Colors[ImGuiCol_CheckMark] = luigiGreenLight;                         // Bright Luigi green for checkmarks
	style.Colors[ImGuiCol_SliderGrab] = luigiGreen;                             // Luigi green for sliders
	style.Colors[ImGuiCol_SliderGrabActive] = luigiGreenLight;                  // Lighter when active

	// Headers (collapsing headers, tree nodes) - GameCube purple
	style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.12f, 0.35f, 1.00f);         // GameCube purple (fully opaque)
	style.Colors[ImGuiCol_HeaderHovered] = gamecubePurple;                      // Full purple when hovered
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.45f, 0.25f, 0.75f, 1.00f);   // Brighter purple when active

	// Tab elements - Red/green for Mario/Luigi contrast
	style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.10f, 0.30f, 1.00f);            // Dark purple tabs (fully opaque)
	style.Colors[ImGuiCol_TabHovered] = luigiGreenLight;                        // Luigi green when hovered
	style.Colors[ImGuiCol_TabActive] = nintendoRed;                             // Mario red when active
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.08f, 0.20f, 1.00f);   // Darker when unfocused (fully opaque)
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.40f, 0.10f, 0.10f, 1.00f); // Darker red when unfocused but active (fully opaque)

	// Border and separator
	style.Colors[ImGuiCol_Border] = ImVec4(0.40f, 0.40f, 0.50f, 0.50f);         // Keep subtle border transparency
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);   // No shadows
	style.Colors[ImGuiCol_Separator] = nintendoRed;                             // Nintendo red separators

	// Table elements
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.15f, 0.10f, 0.25f, 1.00f);      // Dark purple headers (fully opaque)
	style.Colors[ImGuiCol_TableBorderStrong] = nintendoRed;                         // Nintendo red strong borders
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.40f, 0.10f, 0.10f, 0.70f);   // Lighter red inner borders (keep transparency)
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.03f, 0.03f, 0.10f, 1.00f);         // Match window background (fully opaque)
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.06f, 0.06f, 0.14f, 1.00f);      // Slightly lighter for alt rows (fully opaque)

	// Selected text - keep transparency for selection highlight
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(luigiGreen.x, luigiGreen.y, luigiGreen.z, 0.35f); // Semi-transparent Luigi green

	// Reset to defaults for these
	style.TabBorderSize = 0.0f;
	style.FrameBorderSize = 0.0f;
}

static void applySoftTheme()
{
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();

	// Soft, blue/turquoise theme that's easy on the eyes
	// Soft text colors
	style.Colors[ImGuiCol_Text] = ImVec4(0.85f, 0.90f, 0.92f, 1.00f);           // Soft blue-white text
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.60f, 0.65f, 0.70f);   // Muted blue-gray for disabled text (some transparency)

	// Soft dark backgrounds with turquoise tint
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.18f, 0.20f, 1.00f);      // Soft dark background with blue tint (fully opaque)
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.20f, 1.00f);       // Match window background (fully opaque)
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.25f, 0.30f, 1.00f);       // Deep turquoise title bar (fully opaque)
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.35f, 0.45f, 1.00f); // Brighter turquoise when active (fully opaque)
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.25f, 0.30f, 0.75f); // Keep some transparency when collapsed
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.17f, 0.21f, 0.24f, 1.00f);     // Slightly lighter than background (fully opaque)

	// Soft scrollbars
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.15f, 0.18f, 0.20f, 1.00f);   // Match window background (fully opaque)
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.45f, 0.50f, 1.00f); // Soft turquoise (fully opaque)
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.55f, 1.00f, 1.00f); // Brighter when hovered (fully opaque)
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.65f, 1.00f, 1.00f);  // Even brighter when active (fully opaque)

	// Soft, muted button colors
	style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.40f, 0.45f, 1.00f);        // Muted turquoise (fully opaque)
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.50f, 0.55f, 1.00f); // Slightly brighter when hovered (fully opaque)
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.60f, 0.65f, 1.00f);  // Even brighter when active (fully opaque)

	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.18f, 0.20f, 1.00f);       // Match window background (fully opaque)

	// Frames (checkboxes, input fields)
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.28f, 0.33f, 1.00f);       // Soft blue-gray frames (fully opaque)
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.35f, 0.40f, 1.00f); // Slightly brighter when hovered (fully opaque)
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.45f, 0.50f, 1.00f);  // Even brighter when active (fully opaque)

	// Headers (collapsing headers, tree nodes)
	style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.35f, 0.45f, 1.00f);        // Soft blue headers (fully opaque)
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.40f, 0.50f, 1.00f); // Slightly brighter when hovered (fully opaque)
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.50f, 0.60f, 1.00f);  // Even brighter when active (fully opaque)

	// Accent colors - light turquoise
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.80f, 0.90f, 1.00f);     // Light turquoise checkmarks (fully opaque)
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.65f, 0.75f, 1.00f);    // Turquoise sliders (fully opaque)
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.75f, 0.85f, 1.00f); // Brighter when active (fully opaque)

	// Tabs - soft blue palette
	style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.30f, 0.40f, 1.00f);           // Soft blue tabs (fully opaque)
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.40f, 0.50f, 1.00f);    // Slightly brighter when hovered (fully opaque)
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.30f, 0.50f, 0.60f, 1.00f);     // Turquoise blue when active (fully opaque)
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.25f, 0.35f, 1.00f);  // More muted when unfocused (fully opaque)
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.35f, 0.45f, 1.00f); // In-between for unfocused active (fully opaque)

	// Other elements
	style.Colors[ImGuiCol_Border] = ImVec4(0.20f, 0.35f, 0.45f, 0.60f);         // Soft blue borders (some transparency ok)
	style.Colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.35f, 0.45f, 0.75f);      // Soft blue separators (some transparency ok)
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.30f, 0.50f, 0.60f, 0.35f); // Soft blue selection (keep transparency)

	// Table colors
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.15f, 0.30f, 0.40f, 1.00f);  // Soft blue headers (fully opaque)
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.20f, 0.35f, 0.45f, 1.00f); // Soft blue borders (fully opaque)
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.20f, 0.35f, 0.45f, 0.70f);  // Lighter borders (some transparency)
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.15f, 0.18f, 0.20f, 1.00f);     // Match window bg (fully opaque)
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.17f, 0.21f, 0.24f, 1.00f);  // Slightly lighter for alt rows (fully opaque)

	// Reset to defaults for these
	style.TabBorderSize = 0.0f;
	style.FrameBorderSize = 0.0f;
}

// Add the common function before gui_initFonts
void applyCurrentTheme()
{
	if (config::UITheme == 0)
		applyDarkTheme();
	else if (config::UITheme == 1)
		applyLightTheme();
	else if (config::UITheme == 2)
		applyDreamcastTheme();
	else if (config::UITheme == 3)
		applyHighContrastTheme();
	else if (config::UITheme == 4)
		applyNintendoTheme();      // Fixed ordering - was High Contrast
	else if (config::UITheme == 5)
		applySoftTheme();          // New "Aqua Chill" theme
	else
		applyDarkTheme();
}
