//
//  EmuGLView.swift
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

import Cocoa

class EmuGLView: NSOpenGLView, NSWindowDelegate {

    var backingRect: NSRect?
    var swapOnVSync = emu_vsync_enabled()
    
    override var acceptsFirstResponder: Bool {
        return true;
    }
    
    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        backingRect = convertToBacking(dirtyRect)
        
        if swapOnVSync {
            draw()
        }
    }
    
    func draw() {
        if swapOnVSync == (emu_fast_forward() || !emu_vsync_enabled()) {
            swapOnVSync = (!emu_fast_forward() && emu_vsync_enabled())
            var sync: GLint = swapOnVSync ? 1 : 0
            CGLSetParameter(openGLContext!.cglContextObj!, kCGLCPSwapInterval, &sync)
        }
        
        if let backingRect = backingRect {
            openGLContext!.makeCurrentContext()
            if emu_single_frame(Int32(backingRect.width), Int32(backingRect.height)) {
                openGLContext!.flushBuffer() //Swap for macOS
            }
        }
    }
    
    override func awakeFromNib() {
		//self.wantsBestResolutionOpenGLSurface = true
        let renderTimer = Timer.scheduledTimer(timeInterval: 0.001, target: self, selector: #selector(EmuGLView.timerTick), userInfo: nil, repeats: true)
        
		RunLoop.current.add(renderTimer, forMode: .default)
		RunLoop.current.add(renderTimer, forMode: .eventTracking)
        
        let attrs:[NSOpenGLPixelFormatAttribute] =
        [
                UInt32(NSOpenGLPFADoubleBuffer),
                UInt32(NSOpenGLPFADepthSize), UInt32(24),
                UInt32(NSOpenGLPFAStencilSize), UInt32(8),
                // Must specify the 3.2 Core Profile to use OpenGL 3.2
                UInt32(NSOpenGLPFAOpenGLProfile),
                UInt32(NSOpenGLProfileVersion3_2Core),
				UInt32(NSOpenGLPFABackingStore), UInt32(truncating: true),
                UInt32(0)
        ]
        
        let pf = NSOpenGLPixelFormat(attributes:attrs)
        
        let context = NSOpenGLContext(format: pf!, share: nil)

        self.pixelFormat = pf
        self.openGLContext = context
        
        openGLContext!.makeCurrentContext()
		let rect = convertToBacking(frame)
        emu_gles_init(Int32(rect.width), Int32(rect.height))
		
		if (emu_reicast_init() != 0) {
			let alert = NSAlert()
			alert.alertStyle = .critical
			alert.messageText = "Flycast initialization failed"
			alert.runModal()
		}
    }
    
   
    @objc func timerTick() {
		if (!emu_renderer_enabled()) {
			NSApplication.shared.terminate(self)
		}
        else if emu_frame_pending() {
            if swapOnVSync {
                self.needsDisplay = true
            } else {
                self.draw()
            }
        }
    }
    
    override func keyDown(with e: NSEvent) {
		if (!e.isARepeat)
		{
			emu_key_input(e.keyCode, true, UInt32(e.modifierFlags.rawValue & NSEvent.ModifierFlags.deviceIndependentFlagsMask.rawValue))
		}
		emu_character_input(e.characters)
    }
    
    override func keyUp(with e: NSEvent) {
        emu_key_input(e.keyCode, false, UInt32(e.modifierFlags.rawValue & NSEvent.ModifierFlags.deviceIndependentFlagsMask.rawValue))
    }
	
	override func flagsChanged(with e: NSEvent) {
		emu_key_input(0xFF, false, UInt32(e.modifierFlags.rawValue & NSEvent.ModifierFlags.deviceIndependentFlagsMask.rawValue))
	}

	private func setMousePos(_ event: NSEvent)
	{
		let point = convertToBacking(convert(event.locationInWindow, from: self))
		let size = convertToBacking(frame.size)
		emu_set_mouse_position(Int32(point.x), Int32(size.height - point.y), Int32(size.width), Int32(size.height))
	}
	override func mouseDown(with event: NSEvent) {
		emu_mouse_buttons(1, true)
		setMousePos(event)
	}
	override func mouseUp(with event: NSEvent) {
		emu_mouse_buttons(1, false)
		setMousePos(event)
	}
	override func rightMouseDown(with event: NSEvent) {
		emu_mouse_buttons(2, true)
		setMousePos(event)
	}
	override func rightMouseUp(with event: NSEvent) {
		emu_mouse_buttons(2, false)
		setMousePos(event)
	}
	// Not dispatched by default. Need to set Window.acceptsMouseMovedEvents to true
	override func mouseMoved(with event: NSEvent) {
		setMousePos(event)
	}
	override func mouseDragged(with event: NSEvent) {
		emu_mouse_buttons(1, true)
		setMousePos(event)
	}
	override func rightMouseDragged(with event: NSEvent) {
		emu_mouse_buttons(2, true)
		setMousePos(event)
	}
	override func otherMouseDown(with event: NSEvent) {
		emu_mouse_buttons(3, true)
		setMousePos(event)
	}
	override func otherMouseUp(with event: NSEvent) {
		emu_mouse_buttons(3, false)
		setMousePos(event)
	}
	override func scrollWheel(with event: NSEvent) {
		if (event.hasPreciseScrollingDeltas) {
            emu_mouse_wheel(-Float(event.scrollingDeltaY) / 5)
		} else {
            emu_mouse_wheel(-Float(event.scrollingDeltaY) * 10)
		}
	}
	
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        self.window!.delegate = self
		self.window!.acceptsMouseMovedEvents = true
    }
	
	@IBAction func openMenu(_ sender: Any) {
		emu_gui_open_settings();
	}
}
