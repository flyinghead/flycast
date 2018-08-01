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

        // Drawing code here.
        // screen_width = view.drawableWidth;
        // screen_height = view.drawableHeight;
        
        //glClearColor(0.65f, 0.65f, 0.65f, 1.0f);
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        openGLContext!.makeCurrentContext()
        
        if (emu_single_frame(Int32(dirtyRect.width), Int32(dirtyRect.height)) != 0)
        {
            openGLContext!.flushBuffer()
        }
    }
    
    override func awakeFromNib() {
        let renderTimer = Timer.scheduledTimer(timeInterval: 0.001, target: self, selector: #selector(EmuGLView.timerTick), userInfo: nil, repeats: true)
        
        RunLoop.current.add(renderTimer, forMode: RunLoopMode.defaultRunLoopMode);
        RunLoop.current.add(renderTimer, forMode: RunLoopMode.eventTrackingRunLoopMode);
        
        let attrs:[NSOpenGLPixelFormatAttribute] =
        [
                UInt32(NSOpenGLPFADoubleBuffer),
                UInt32(NSOpenGLPFADepthSize), UInt32(24),
                UInt32(NSOpenGLPFAStencilSize), UInt32(8),
                // Must specify the 3.2 Core Profile to use OpenGL 3.2
                UInt32(NSOpenGLPFAOpenGLProfile),
                UInt32(NSOpenGLProfileVersion3_2Core),
                UInt32(0)
        ]
        
        let pf = NSOpenGLPixelFormat(attributes:attrs)
        
        let context = NSOpenGLContext(format: pf!, share: nil);
        
        self.pixelFormat = pf;
        self.openGLContext = context;
        
        openGLContext!.makeCurrentContext()
        emu_gles_init();
    }
    
   
    func timerTick() {
        if (emu_frame_pending())
        {
            self.needsDisplay = true;
        }
    }
    
    override func keyDown(with e: NSEvent) {
        emu_key_input(e.keyCode, 1);
    }
    
    override func keyUp(with e: NSEvent) {
        emu_key_input(e.keyCode, 0);
    }
    
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        self.window!.delegate = self
    }
    
    func windowWillClose(_ notification: Notification) {
        emu_dc_stop()
    }
}
