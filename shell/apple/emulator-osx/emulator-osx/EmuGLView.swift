//
//  EmuGLView.swift
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

import Cocoa

class EmuGLView: NSOpenGLView, NSWindowDelegate {

    override var acceptsFirstResponder: Bool {
        return true;
    }
    
    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)

        openGLContext!.makeCurrentContext()
        
        let rect = convertToBacking(dirtyRect)
        if (emu_single_frame(Int32(rect.width), Int32(rect.height)) != 0) {
            openGLContext!.flushBuffer()
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
        else if (emu_frame_pending()) {
            self.needsDisplay = true
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

	private func setMousePos(_ event: NSEvent)
	{
		let point = convertToBacking(convert(event.locationInWindow, from: self))
		let size = convertToBacking(frame.size)
		emu_set_mouse_position(Int32(point.x), Int32(size.height - point.y), Int32(size.width), Int32(size.height))
	}
	override func mouseDown(with event: NSEvent) {
		emu_mouse_buttons(1, true)
		mo_buttons &= ~(1 << 2)
		setMousePos(event)
	}
	override func mouseUp(with event: NSEvent) {
		emu_mouse_buttons(1, false)
		mo_buttons |= 1 << 2;
		setMousePos(event)
	}
	override func rightMouseDown(with event: NSEvent) {
		emu_mouse_buttons(2, true)
		mo_buttons &= ~(1 << 1)
		setMousePos(event)
	}
	override func rightMouseUp(with event: NSEvent) {
		emu_mouse_buttons(2, false)
		mo_buttons |= 1 << 1
		setMousePos(event)
	}
	// Not dispatched by default. Need to set Window.acceptsMouseMovedEvents to true
	override func mouseMoved(with event: NSEvent) {
		setMousePos(event)
	}
	override func mouseDragged(with event: NSEvent) {
		mo_buttons &= ~(1 << 2)
		setMousePos(event)
	}
	override func rightMouseDragged(with event: NSEvent) {
		mo_buttons &= ~(1 << 1)
		setMousePos(event)
	}
	override func otherMouseDown(with event: NSEvent) {
		emu_mouse_buttons(3, true)
		mo_buttons &= ~(1 << 2)
		setMousePos(event)
	}
	override func otherMouseUp(with event: NSEvent) {
		emu_mouse_buttons(3, false)
		mo_buttons |= 1 << 2
		setMousePos(event)
	}
	override func scrollWheel(with event: NSEvent) {
		if (event.hasPreciseScrollingDeltas) {
			// 1 per "line"
			mo_wheel_delta -= Float(event.scrollingDeltaY) * 3.2
		} else {
			// 0.1 per wheel notch
			mo_wheel_delta -= Float(event.scrollingDeltaY) * 160
		}
	}
	
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        self.window!.delegate = self
		self.window!.acceptsMouseMovedEvents = true
    }
}
