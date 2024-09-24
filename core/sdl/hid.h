#pragma once
void hidInit();
void hidTerm();
void hidInput();
void hidOutput(bool play, bool spotL, bool spotR, bool backL, bool backR);

extern int jogWheelsDelta[2];
extern bool jogWheelsTouched[2];
extern short crossFader;
