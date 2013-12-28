/*
	Config file crap
	Supports various things, as virtual config entries and such crap
	Works surprisingly well considering how old it is ...
*/

#define _CRT_SECURE_NO_DEPRECATE (1)
#include "cfg.h"

string cfgPath;

//A config remains virtual only as long as a write at it
//doesn't override the virtual value.While a config is virtual, a copy of its 'real' value is held and preserved

//Is this a virtual entry ?
#define CEM_VIRTUAL 1
//Should the value be saved ?
#define CEM_SAVE	2 
//is this entry readonly ? 
#define CEM_READONLY 4
//the move is from loading ?
#define CEM_LOAD 8

struct ConfigEntry
{
	ConfigEntry(ConfigEntry* pp)
	{
		next=pp;
		flags=0;
	}


	u32 flags;
	string name;
	string value;
	string valueVirtual;
	ConfigEntry* next;
	void SaveFile(FILE* file)
	{
		if (flags & CEM_SAVE)
			fprintf(file,"%s=%s\n",name.c_str(),value.c_str());
	} 

	string GetValue()
	{
		if (flags&CEM_VIRTUAL)
			return valueVirtual;
		else
			return value;
	}
};
struct ConfigSection
{
	u32 flags;
	string name;
	ConfigEntry* entrys;
	ConfigSection* next;
	
	ConfigSection(ConfigSection* pp)
	{
		next=pp;
		flags=0;
		entrys=0;
	}
	ConfigEntry* FindEntry(string name)
	{
		ConfigEntry* c=	entrys;
		while(c)
		{
			if (stricmp(name.c_str(),c->name.c_str())==0)
				return c;
			c=c->next;
		}
		return 0;
	}
	void SetEntry(string name,string value,u32 eflags)
	{
		ConfigEntry* c=FindEntry(name);
		if (c)
		{
			//readonly is read only =)
			if (c->flags & CEM_READONLY)
				return;

			//virtual : save only if different value
			if (c->flags & CEM_VIRTUAL)
			{

				if(stricmp(c->valueVirtual.c_str(),value.c_str())==0)
					return;
				c->flags&=~CEM_VIRTUAL;
			}
		}
		else
		{
			entrys=c= new ConfigEntry(entrys);
			c->name=name;
		}

		verify(!(c->flags&(CEM_VIRTUAL|CEM_READONLY)));
		//Virtual
		//Virtual | ReadOnly
		//Save
		if (eflags & CEM_VIRTUAL)
		{
			verify(!(eflags & CEM_SAVE));
			c->flags|=eflags;
			c->valueVirtual=value;
		}
		else if (eflags & CEM_SAVE)
		{
			verify(!(eflags & (CEM_VIRTUAL|CEM_READONLY)));
			flags|=CEM_SAVE;
			c->flags|=CEM_SAVE;

			c->value=value;
		}
		else
		{
			die("Invalid eflags value");
		}
		
	}
	~ConfigSection()
	{
		ConfigEntry* n=entrys;
		
		while(n)
		{
			ConfigEntry* p=n;	
			n=n->next;
			delete p;
		}
	}
	void SaveFile(FILE* file)
	{
		if (flags&CEM_SAVE)
		{
			fprintf(file,"[%s]\n",name.c_str());

			vector<ConfigEntry*> stuff;

			ConfigEntry* n=entrys;

			while(n)
			{
				stuff.push_back(n);
				n=n->next;
			}

			for (int i=stuff.size()-1;i>=0;i--)
			{
				stuff[i]->SaveFile(file);
			}

			fprintf(file,"\n");
		}
	}

};
struct ConfigFile
{
	ConfigSection* entrys;
	ConfigSection* FindSection(string name)
	{
		ConfigSection* c=	entrys;
		while(c)
		{
			if (stricmp(name.c_str(),c->name.c_str())==0)
				return c;
			c=c->next;
		}
		return 0;
	}
	ConfigSection* GetEntry(string name)
	{
		ConfigSection* c=FindSection(name);
		if (!c)
		{
			entrys=c= new ConfigSection(entrys);
			c->name=name;
		}

		return c;
	}
	~ConfigFile()
	{
		ConfigSection* n=entrys;
		
		while(n)
		{
			ConfigSection* p=n;	
			n=n->next;
			delete p;
		}
	}

	void ParseFile(FILE* file)
	{
		
	}
	void SaveFile(FILE* file)
	{
		vector<ConfigSection*> stuff;

		ConfigSection* n=entrys;
		
		while(n)
		{
			stuff.push_back(n);
			n=n->next;
		}

		for (int i=stuff.size()-1;i>=0;i--)
		{
			if (stuff[i]->name!="emu")
				stuff[i]->SaveFile(file);
		}
	}
};

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
struct vitem
{
	string s;
	string n;
	string v;
	vitem(string a,string b,string c){s=a;n=b;v=c;}
};
vector<vitem> vlist;
wchar* trim_ws(wchar* str);

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

	wchar line[512];
	wchar cur_sect[512]={0};
	int cline=0;
	while(cfgfile && !feof(cfgfile))
	{
		fgets(line,512,cfgfile);
		if (feof(cfgfile))
			break;

		cline++;

		if (strlen(line)<3)
			continue;
		if (line[strlen(line)-1]=='\r' || line[strlen(line)-1]=='\n')
			line[strlen(line)-1]=0;

		wchar* tl=trim_ws(line);
		if (tl[0]=='[' && tl[strlen(tl)-1]==']')
		{
			tl[strlen(tl)-1]=0;
			strcpy(cur_sect,tl+1);
			trim_ws(cur_sect);
		}
		else
		{
			if (cur_sect[0]==0)
				continue;//no open section
			wchar* str1=strstr(tl,"=");
			if (!str1)
			{
				printf("Malformed entry on config - ignoring @ %d(%s)\n",cline,tl);
				continue;
			}
			*str1=0;
			str1++;
			wchar* v=trim_ws(str1);
			wchar* k=trim_ws(tl);
			if (v && k)
			{
				ConfigSection*cs=cfgdb.GetEntry(cur_sect);
				
				//if (!cs->FindEntry(k))
				cs->SetEntry(k,v,CEM_SAVE|CEM_LOAD);
			}
			else
			{
				printf("Malformed entry on config - ignoring @ %d(%s)\n",cline,tl);
			}
		}
	}

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
	if (Section==0)
		return -1;
	//return cfgRead(Section,Key,0);
	ConfigSection*cs= cfgdb.FindSection(Section);
	if (cs ==  0)
		return 0;

	if (Key==0)
		return 1;

	ConfigEntry* ce=cs->FindEntry(Key);
	if (ce!=0)
		return 2;
	else
		return 0;
}
void  cfgLoadStr(const wchar * Section, const wchar * Key, wchar * Return,const wchar* Default)
{
	verify(Section!=0 && strlen(Section)!=0);
	verify(Key!=0 && strlen(Key)!=0);
	verify(Return!=0);
	if (Default==0)
		Default="";
	ConfigSection* cs= cfgdb.GetEntry(Section);
	ConfigEntry* ce=cs->FindEntry(Key);
	if (!ce)
	{
		cs->SetEntry(Key,Default,CEM_SAVE);
		strcpy(Return,Default);
	}
	else
	{
		strcpy(Return,ce->GetValue().c_str());
	}
}

//These are helpers , mainly :)
s32  cfgLoadInt(const wchar * Section, const wchar * Key,s32 Default)
{
	wchar temp_d[30];
	wchar temp_o[30];
	sprintf(temp_d,"%d",Default);
	cfgLoadStr(Section,Key,temp_o,temp_d);
	return atoi(temp_o);
}

void  cfgSaveInt(const wchar * Section, const wchar * Key, s32 Int)
{
	wchar tmp[32];
	sprintf(tmp,"%d", Int);
	cfgSaveStr(Section,Key,tmp);
}
void cfgSetVitual(const wchar * Section, const wchar * Key, const wchar * String)
{
	vlist.push_back(vitem(Section,Key,String));
	//cfgdb.GetEntry(Section,CEM_VIRTUAL)->SetEntry(Key,String,CEM_VIRTUAL);
}