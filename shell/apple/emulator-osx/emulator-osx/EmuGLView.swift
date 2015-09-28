//
//  EmuGLView.swift
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

import Cocoa

class EmuGLView: NSOpenGLView {

    override var acceptsFirstResponder: Bool {
        return true;
    }
    
    override func drawRect(dirtyRect: NSRect) {
        super.drawRect(dirtyRect)

        // Drawing code here.
        // screen_width = view.drawableWidth;
        // screen_height = view.drawableHeight;
        
        //glClearColor(0.65f, 0.65f, 0.65f, 1.0f);
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        openGLContext!.makeCurrentContext()
        
        while !emu_single_frame(Int32(dirtyRect.width), Int32(dirtyRect.height)) { }
        
        openGLContext!.flushBuffer()
    }
    
    override func awakeFromNib() {
        var renderTimer = NSTimer.scheduledTimerWithTimeInterval(0.001, target: self, selector: Selector("timerTick"), userInfo: nil, repeats: true)
        
        NSRunLoop.currentRunLoop().addTimer(renderTimer, forMode: NSDefaultRunLoopMode);
        NSRunLoop.currentRunLoop().addTimer(renderTimer, forMode: NSEventTrackingRunLoopMode);
        
        let attrs:[NSOpenGLPixelFormatAttribute] =
        [
                UInt32(NSOpenGLPFADoubleBuffer),
                UInt32(NSOpenGLPFADepthSize), UInt32(24),
                // Must specify the 3.2 Core Profile to use OpenGL 3.2
                UInt32(NSOpenGLPFAOpenGLProfile),
                UInt32(NSOpenGLProfileVersion3_2Core),
                UInt32(0)
        ]
        
        let pf = NSOpenGLPixelFormat(attributes:attrs)
        
        let context = NSOpenGLContext(format: pf!, shareContext: nil);
        
        self.pixelFormat = pf;
        self.openGLContext = context;
        
        openGLContext!.makeCurrentContext()
        emu_gles_init();
    }
    
   
    func timerTick() {
        self.needsDisplay = true;
    }
    
    override func keyDown(e: NSEvent) {
        emu_key_input(e.characters!, 1);
    }
    
    override func keyUp(e: NSEvent) {
        emu_key_input(e.characters!, 0);
    }
    
}
