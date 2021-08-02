// nullAICA.cpp : Defines the entry point for the DLL application.
//

#include "arm7.h"

//called when plugin is used by emu (you should do first time init here)
s32 libARM_Init()
{
	aicaarm::init();

	return 0;
}

//called when plugin is unloaded by emu, only if dcInit is called (eg, not called to enumerate plugins)
void libARM_Term()
{
	//arm7_Term ?
}

//It's supposed to reset anything
void libARM_Reset(bool hard)
{
	aicaarm::reset();
	aicaarm::enable(false);
}
