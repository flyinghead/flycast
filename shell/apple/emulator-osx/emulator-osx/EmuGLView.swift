//
//  EmuGLView.swift
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

import Cocoa

class EmuGLView: NSOpenGLView, NSWindowDelegate {

	var mouse_prev_x: Int32 = 0
	var mouse_prev_y: Int32 = 0
	
    override var acceptsFirstResponder: Bool {
        return true;
    }
    
    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)

        openGLContext!.makeCurrentContext()
        
        if (emu_single_frame(Int32(dirtyRect.width), Int32(dirtyRect.height)) != 0)
        {
            openGLContext!.flushBuffer()
        }
    }
    
    override func awakeFromNib() {
        let renderTimer = Timer.scheduledTimer(timeInterval: 0.001, target: self, selector: #selector(EmuGLView.timerTick), userInfo: nil, repeats: true)
        
        RunLoop.current.add(renderTimer, forMode: RunLoopMode.defaultRunLoopMode)
        RunLoop.current.add(renderTimer, forMode: RunLoopMode.eventTrackingRunLoopMode)
        
        let attrs:[NSOpenGLPixelFormatAttribute] =
        [
                UInt32(NSOpenGLPFADoubleBuffer),
                UInt32(NSOpenGLPFADepthSize), UInt32(24),
                UInt32(NSOpenGLPFAStencilSize), UInt32(8),
                // Must specify the 3.2 Core Profile to use OpenGL 3.2
                UInt32(NSOpenGLPFAOpenGLProfile),
                UInt32(NSOpenGLProfileVersion3_2Core),
                UInt32(NSOpenGLPFABackingStore), UInt32(true),
                UInt32(0)
        ]
        
        let pf = NSOpenGLPixelFormat(attributes:attrs)
        
        let context = NSOpenGLContext(format: pf!, share: nil)

        self.pixelFormat = pf
        self.openGLContext = context
        
        openGLContext!.makeCurrentContext()
        emu_gles_init(Int32(frame.width), Int32(frame.height))
		
		if (emu_reicast_init() != 0) {
			let alert = NSAlert()
			alert.alertStyle = .critical
			alert.messageText = "Reicast initialization failed"
			alert.runModal()
		}
    }
    
   
    func timerTick() {
        if (emu_frame_pending())
        {
            self.needsDisplay = true
        }
    }
    
    override func keyDown(with e: NSEvent) {
		if (!e.isARepeat)
		{
			emu_key_input(e.keyCode, true, UInt32(e.modifierFlags.rawValue & NSDeviceIndependentModifierFlagsMask.rawValue))
		}
		emu_character_input(e.characters)
    }
    
    override func keyUp(with e: NSEvent) {
        emu_key_input(e.keyCode, false, UInt32(e.modifierFlags.rawValue & NSDeviceIndependentModifierFlagsMask.rawValue))
    }

	private func setMousePos(_ event: NSEvent)
	{
		let point = convert(event.locationInWindow, from: self)
		let size = frame.size
		let scale = 480.0 / size.height
		mo_x_abs = Int32((point.x - (size.width - 640.0 / scale) / 2.0) * scale)
		mo_y_abs = Int32((size.height - point.y) * scale)
		mo_x_delta += Float(mo_x_abs - mouse_prev_x)
		mo_y_delta += Float(mo_y_abs - mouse_prev_y)
		mouse_prev_x = mo_x_abs
		mouse_prev_y = mo_y_abs
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
    
    func windowWillClose(_ notification: Notification) {
        emu_dc_exit()
    }
}
