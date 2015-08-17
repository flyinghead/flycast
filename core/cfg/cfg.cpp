/*
	Config file crap
	Supports various things, as virtual config entries and such crap
	Works surprisingly well considering how old it is ...
*/

#define _CRT_SECURE_NO_DEPRECATE (1)
#include "cfg.h"
#include "ini.h"

string cfgPath;

struct vitem
{
	string s;
	string n;
	string v;
	vitem(string a,string b,string c){s=a;n=b;v=c;}
};
vector<vitem> vlist;

ConfigFile cfgdb;

void savecfgf()
{
	FILE* cfgfile = fopen(cfgPath.c_str(),"wt");
	if (!cfgfile)
		printf("Error : Unable to open file for saving \n");
	else
	{
		cfgdb.SaveFile(cfgfile);
		fclose(cfgfile);
	}
}
void  cfgSaveStr(const wchar * Section, const wchar * Key, const wchar * String)
{
	cfgdb.GetEntry(Section)->SetEntry(Key,String,CEM_SAVE);
	savecfgf();
	//WritePrivateProfileString(Section,Key,String,cfgPath);
}
//New config code

/*
	I want config to be really flexible .. so , here is the new implementation :
	
	Functions :
	cfgLoadInt  : Load an int , if it does not exist save the default value to it and return it
	cfgSaveInt  : Save an int
	cfgLoadStr  : Load a str , if it does not exist save the default value to it and return it
	cfgSaveStr  : Save a str
	cfgExists   : Returns true if the Section:Key exists. If Key is null , it retuns true if Section exists

	Config parameters can be read from the config file , and can be given at the command line
	-cfg section:key=value -> defines a value at command line
	If a cfgSave* is made on a value defined by command line , then the command line value is replaced by it

	cfg values set by command line are not written to the cfg file , unless a cfgSave* is used

	There are some special values , all of em are on the emu namespace :)

	These are readonly :

	emu:AppPath		: Returns the path where the emulator is stored
	emu:PluginPath	: Returns the path where the plugins are loaded from
	emu:DataPath	: Returns the path where the bios/data files are

	emu:FullName	: str,returns the emulator's name + version string (ex."nullDC v1.0.0 Private Beta 2 built on {datetime}")
	emu:ShortName	: str,returns the emulator's name + version string , short form (ex."nullDC 1.0.0pb2")
	emu:Name		: str,returns the emulator's name (ex."nullDC")

	These are read/write
	emu:Caption		: str , get/set the window caption
*/

///////////////////////////////
/*
**	This will verify there is a working file @ ./szIniFn
**	- if not present, it will write defaults
*/

bool cfgOpen()
{
	cfgPath=GetPath("/emu.cfg");
	FILE* cfgfile = fopen(cfgPath.c_str(),"r");
	if(!cfgfile) {
		cfgfile = fopen(cfgPath.c_str(),"wt");
		if(!cfgfile) 
			printf("Unable to open the config file for reading or writing\nfile : %s\n",cfgPath.c_str());
		else
		{
			fseek(cfgfile,0,SEEK_SET);
			fclose(cfgfile);
			cfgfile = fopen(cfgPath.c_str(),"r");
			if(!cfgfile) 
				printf("Unable to open the config file for reading\nfile : %s\n",cfgPath.c_str());
		}
	}

	cfgdb.ParseFile(cfgfile);

	for (size_t i=0;i<vlist.size();i++)
	{
		cfgdb.GetEntry(vlist[i].s)->SetEntry(vlist[i].n,vlist[i].v,CEM_VIRTUAL);
	}

	if (cfgfile)
	{
		cfgdb.SaveFile(cfgfile);
		fclose(cfgfile);
	}
	return true;
}

//Implementations of the interface :)
//Section must be set
//If key is 0 , it looks for the section
//0 : not found
//1 : found section , key was 0
//2 : found section & key
s32  cfgExists(const wchar * Section, const wchar * Key)
{
	return cfgdb.Exists(Section, Key);
}
void  cfgLoadStr(const wchar * Section, const wchar * Key, wchar * Return,const wchar* Default)
{
	return cfgdb.LoadStr(Section, Key, Return, Default);
}

string  cfgLoadStr(const wchar * Section, const wchar * Key, const wchar* Default)
{
	return cfgdb.LoadStr(Section, Key, Default);
}

//These are helpers , mainly :)
s32  cfgLoadInt(const wchar * Section, const wchar * Key,s32 Default)
{
	return cfgdb.LoadInt(Section, Key, Default);
}

void  cfgSaveInt(const wchar * Section, const wchar * Key, s32 Int)
{
	wchar tmp[32];
	sprintf(tmp,"%d", Int);
	cfgSaveStr(Section,Key,tmp);
}
void cfgSetVirtual(const wchar * Section, const wchar * Key, const wchar * String)
{
	vlist.push_back(vitem(Section,Key,String));
	//cfgdb.GetEntry(Section,CEM_VIRTUAL)->SetEntry(Key,String,CEM_VIRTUAL);
}
