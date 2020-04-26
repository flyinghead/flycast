//
//  AppDelegate.swift
//  emulator-osx
//
//  Created by admin on 6/1/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

    @IBOutlet weak var window: NSWindow!


    func applicationDidFinishLaunching(_ aNotification: Notification) {
		if let name = Bundle.main.infoDictionary?["CFBundleDisplayName"] as? String {
			window.title = name
		}
        NSApplication.shared.mainMenu?.item(at: 1)?.submenu?.insertItem(
            NSMenuItem(title: "New Instance", action: #selector(newInstance(_:)), keyEquivalent: "n"), at: 0
        )
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        emu_dc_term()
    }
    
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        return true
    }
    
    func applicationDockMenu(_ sender: NSApplication) -> NSMenu? {
        let dockMenu = NSMenu()
        dockMenu.addItem(withTitle: "New Instance", action: #selector(newInstance(_:)), keyEquivalent: "n")
        return dockMenu
    }
    
    @objc func newInstance(_ sender: NSMenuItem) {
        Process.launchedProcess(launchPath: "/usr/bin/open", arguments: ["-n", Bundle.main.bundlePath])
    }
}

