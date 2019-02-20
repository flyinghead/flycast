package com.reicast.emulator.emu;

public interface IEmulatorView {
    boolean hasSound();
    void reiosInfo(String reiosId, String reiosSoftware);
    void postMessage(String msg);
    void finish();
}
