/*
    This file is part of Flycast.
    Flycast is free software under the GNU General Public License, version 2
    or (at your option) any later version.
*/
import AppKit
import Combine
import SwiftUI

private func L(_ key: String) -> String {
    NSLocalizedString(key, comment: "")
}

private struct LibraryGame: Decodable, Identifiable, Equatable, Sendable {
    let name: String
    let path: String
    let artwork: String
    let arcade: Bool

    var id: String { path }
}

@MainActor
private final class LibraryModel: ObservableObject {
    @Published var games: [LibraryGame] = []
    @Published var folders: [String] = []
    @Published var launchFullscreen = true
    @Published var widescreen = false
    @Published var widescreenHacks = false
    @Published var resolution = 480
    private var observations: [AnyCancellable] = []
    private var refreshTask: Task<Void, Never>?
    private var isObserving = false
    private var pendingUpdate = false
    private var pendingArtworkEnqueue = false
    private var generation = 0

    func start() {
        guard !isObserving else { return }
        isObserving = true
        observations = [
            NotificationCenter.default.publisher(for: Notification.Name("FlycastNativeScannerDidChange"))
                .sink { [weak self] _ in self?.requestUpdate(enqueueArtwork: true) },
            NotificationCenter.default.publisher(for: Notification.Name("FlycastNativeArtworkDidChange"))
                .sink { [weak self] _ in self?.requestUpdate(enqueueArtwork: false) }
        ]
        FlycastNativeObserveLibrary(true)
        refreshSettings()
        requestUpdate(enqueueArtwork: true)
    }

    func stop() {
        isObserving = false
        FlycastNativeObserveLibrary(false)
        observations.removeAll()
        generation += 1
        refreshTask?.cancel()
        refreshTask = nil
        pendingUpdate = false
        pendingArtworkEnqueue = false
    }

    private func requestUpdate(enqueueArtwork: Bool) {
        guard isObserving else { return }
        pendingUpdate = true
        pendingArtworkEnqueue = pendingArtworkEnqueue || enqueueArtwork
        guard refreshTask == nil else { return }
        let currentGeneration = generation
        refreshTask = Task { [weak self] in
            guard let self else { return }
            while currentGeneration == generation && pendingUpdate && !Task.isCancelled {
                pendingUpdate = false
                let enqueueArtwork = pendingArtworkEnqueue
                pendingArtworkEnqueue = false
                await loadGames(enqueueArtwork: enqueueArtwork)
            }
            if currentGeneration == generation { refreshTask = nil }
        }
    }

    private func loadGames(enqueueArtwork: Bool) async {
        // The scanner and cover-art database perform I/O. Keep that work away
        // from AppKit's main thread and publish only changed snapshots here.
        let snapshot = await Task.detached(priority: .utility) { () -> [LibraryGame] in
            guard let pointer = FlycastNativeGamesJSON(enqueueArtwork) else { return [] }
            let data = Data(String(cString: pointer).utf8)
            FlycastNativeFree(pointer)
            return (try? JSONDecoder().decode([LibraryGame].self, from: data)) ?? []
        }.value
        guard !Task.isCancelled else { return }
        if snapshot != games { games = snapshot }
        refreshSettings()
    }

    private func refreshSettings() {
        if let pointer = FlycastNativeContentPathsJSON() {
            let data = Data(String(cString: pointer).utf8)
            FlycastNativeFree(pointer)
            if let decoded = try? JSONDecoder().decode([String].self, from: data), decoded != folders {
                folders = decoded
                FlycastNativeWatchContentPaths()
            }
        }
        launchFullscreen = FlycastNativeLaunchFullscreen()
        widescreen = FlycastNativeWidescreen()
        widescreenHacks = FlycastNativeWidescreenHacks()
        resolution = Int(FlycastNativeResolution())
    }

    func addFolder() {
        let panel = NSOpenPanel()
        panel.title = L("select_games_folder")
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        if panel.runModal() == .OK, let url = panel.url {
            url.path.withCString { FlycastNativeAddContentPath($0) }
            refreshSettings()
            requestUpdate(enqueueArtwork: true)
        }
    }

    func play(_ game: LibraryGame) {
        game.path.withCString { FlycastNativeLaunch($0) }
    }
}

private enum SidebarPage: String, CaseIterable, Identifiable {
    case library = "library"
    case settings = "settings"

    var id: String { rawValue }
    var symbol: String { self == .library ? "square.grid.2x2" : "gearshape" }
}

private struct LibraryRootView: View {
    @StateObject private var model = LibraryModel()
    @State private var selection: SidebarPage? = .library
    @State private var search = ""

    private var filteredGames: [LibraryGame] {
        let query = search.trimmingCharacters(in: .whitespacesAndNewlines)
        return query.isEmpty ? model.games : model.games.filter {
            $0.name.localizedStandardContains(query)
        }
    }

    var body: some View {
        NavigationSplitView {
            List(SidebarPage.allCases, selection: $selection) { page in
                Label(L(page.rawValue), systemImage: page.symbol)
                    .tag(page)
            }
            .listStyle(.sidebar)
            .navigationTitle("Flycast")
        } detail: {
            if selection == .settings {
                settingsView
            } else {
                libraryView
            }
        }
        .frame(minWidth: 800, minHeight: 540)
        .onAppear { model.start() }
        .onDisappear { model.stop() }
    }

    private var libraryView: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline) {
                VStack(alignment: .leading, spacing: 4) {
                    Text(L("your_library"))
                        .font(.largeTitle.bold())
                    Text(String(format: L(model.games.count == 1 ? "game_available" : "games_available"), model.games.count))
                        .foregroundStyle(.secondary)
                }
                Spacer()
                Button(action: model.addFolder) {
                    Label(L("add_folder"), systemImage: "folder.badge.plus")
                }
                .buttonStyle(.bordered)
            }
            .padding(.horizontal, 30)
            .padding(.top, 30)
            .padding(.bottom, 20)

            if filteredGames.isEmpty {
                VStack(spacing: 12) {
                    Image(systemName: search.isEmpty ? "square.stack.3d.up.slash" : "magnifyingglass")
                        .font(.system(size: 38, weight: .light))
                        .foregroundStyle(.secondary)
                    Text(L(search.isEmpty ? "library_empty" : "no_results"))
                        .font(.title3.weight(.semibold))
                    Text(L(search.isEmpty ? "add_folder_hint" : "try_another_name"))
                        .foregroundStyle(.secondary)
                    if search.isEmpty {
                        Button(L("add_folder"), action: model.addFolder)
                            .buttonStyle(.borderedProminent)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                ScrollView {
                    LazyVGrid(columns: [GridItem(.adaptive(minimum: 170, maximum: 220), spacing: 22)], spacing: 26) {
                        ForEach(filteredGames) { game in
                            GameCard(game: game) { model.play(game) }
                        }
                    }
                    .padding(.horizontal, 30)
                    .padding(.bottom, 30)
                }
            }
        }
        .searchable(text: $search, placement: .toolbar, prompt: L("search_games"))
        .background(Color(nsColor: .windowBackgroundColor))
    }

    private var settingsView: some View {
        Form {
            Section(L("library")) {
                ForEach(model.folders, id: \.self) { folder in
                    Label(folder, systemImage: "folder")
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
                Button(L("add_folder_ellipsis"), action: model.addFolder)
            }
            Section(L("video")) {
                Toggle(L("launch_fullscreen"), isOn: Binding(
                    get: { model.launchFullscreen },
                    set: { model.launchFullscreen = $0; FlycastNativeSetLaunchFullscreen($0) }
                ))
                Toggle(L("widescreen"), isOn: Binding(
                    get: { model.widescreen },
                    set: { model.widescreen = $0; FlycastNativeSetWidescreen($0) }
                ))
                Toggle(L("widescreen_hacks"), isOn: Binding(
                    get: { model.widescreenHacks },
                    set: { model.widescreenHacks = $0; FlycastNativeSetWidescreenHacks($0) }
                ))
                Picker(L("render_resolution"), selection: Binding(
                    get: { model.resolution },
                    set: { model.resolution = $0; FlycastNativeSetResolution(Int32($0)) }
                )) {
                    Text(L("native_480p")).tag(480)
                    Text(L("double_960p")).tag(960)
                    Text(L("triple_1440p")).tag(1440)
                }
            }
            Section {
                Button(L("all_settings")) {
                    FlycastNativeAdvancedSettings()
                }
            } footer: {
                Text(L("advanced_settings_hint"))
            }
        }
        .formStyle(.grouped)
        .navigationTitle(L("settings"))
    }
}

private struct GameCard: View {
    let game: LibraryGame
    let play: () -> Void

    var body: some View {
        Button(action: play) {
            VStack(alignment: .leading, spacing: 10) {
                ZStack {
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .fill(Color.accentColor.opacity(0.12).gradient)
                    if let image = NSImage(contentsOfFile: game.artwork) {
                        Image(nsImage: image)
                            .resizable()
                            .scaledToFit()
                            .padding(6)
                    } else {
                        Image(systemName: "opticaldisc")
                            .font(.system(size: 54, weight: .ultraLight))
                            .foregroundStyle(.tint)
                    }
                }
                .aspectRatio(1, contentMode: .fit)
                .overlay {
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .strokeBorder(.primary.opacity(0.08))
                }
                Text(game.name)
                    .font(.subheadline.weight(.semibold))
                    .lineLimit(2)
                    .multilineTextAlignment(.leading)
                    .foregroundStyle(.primary)
                Text(L(game.arcade ? "arcade" : "dreamcast"))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .help(String(format: L("play_game"), game.name))
    }
}

@MainActor
@objc(FlycastNativeLibrary)
final class FlycastNativeLibrary: NSObject, NSWindowDelegate {
    private static let shared = FlycastNativeLibrary()
    private var window: NSWindow?

    @objc class func showLibrary() {
        shared.show()
    }

    @objc class func hideLibrary() {
        shared.window?.orderOut(nil)
    }

    private func show() {
        if window == nil {
            let created = NSWindow(
                contentRect: NSRect(x: 0, y: 0, width: 1080, height: 700),
                styleMask: [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView],
                backing: .buffered,
                defer: false
            )
            created.title = "Flycast"
            created.titleVisibility = .hidden
            created.titlebarAppearsTransparent = true
            created.toolbarStyle = .unified
            created.isReleasedWhenClosed = false
            created.contentView = NSHostingView(rootView: LibraryRootView())
            created.center()
            created.delegate = self
            window = created
        }
        window?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func windowWillClose(_ notification: Notification) {
        FlycastNativeQuit()
    }
}
