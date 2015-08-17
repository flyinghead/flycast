#include "ini.h"

wchar* trim_ws(wchar* str);

ConfigEntry::ConfigEntry(ConfigEntry* pp)
{
	next=pp;
	flags=0;
}

void ConfigEntry::SaveFile(FILE* file)
{
	if (flags & CEM_SAVE)
		fprintf(file,"%s=%s\n",name.c_str(),value.c_str());
}

string ConfigEntry::GetValue()
{
	if (flags&CEM_VIRTUAL)
		return valueVirtual;
	else
		return value;
}

ConfigSection::ConfigSection(ConfigSection* pp)
{
	next=pp;
	flags=0;
	entrys=0;
}

ConfigEntry* ConfigSection::FindEntry(string name)
{
	ConfigEntry* c= entrys;
	while(c)
	{
		if (stricmp(name.c_str(),c->name.c_str())==0)
			return c;
		c=c->next;
	}
	return 0;
}

void ConfigSection::SetEntry(string name,string value,u32 eflags)
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

ConfigSection::~ConfigSection()
{
	ConfigEntry* n=entrys;
	
	while(n)
	{
		ConfigEntry* p=n; 
		n=n->next;
		delete p;
	}
}

void ConfigSection::SaveFile(FILE* file)
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

ConfigSection* ConfigFile::FindSection(string name)
{
	ConfigSection* c= entrys;
	while(c)
	{
		if (stricmp(name.c_str(),c->name.c_str())==0)
			return c;
		c=c->next;
	}
	return 0;
}

ConfigSection* ConfigFile::GetEntry(string name)
{
	ConfigSection* c=FindSection(name);
	if (!c)
	{
		entrys=c= new ConfigSection(entrys);
		c->name=name;
	}

	return c;
}

ConfigFile::~ConfigFile()
{
	ConfigSection* n=entrys;
	
	while(n)
	{
		ConfigSection* p=n; 
		n=n->next;
		delete p;
	}
}

void ConfigFile::ParseFile(FILE* file)
{
	wchar line[512];
	wchar cur_sect[512]={0};
	int cline=0;
	while(file && !feof(file))
	{
		fgets(line,512,file);
		if (feof(file))
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
				ConfigSection*cs=this->GetEntry(cur_sect);
				
				//if (!cs->FindEntry(k))
				cs->SetEntry(k,v,CEM_SAVE|CEM_LOAD);
			}
			else
			{
				printf("Malformed entry on config - ignoring @ %d(%s)\n",cline,tl);
			}
		}
	}
}

void ConfigFile::SaveFile(FILE* file)
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

s32 ConfigFile::Exists(const wchar * Section, const wchar * Key)
{
	if (Section==0)
		return -1;
	//return cfgRead(Section,Key,0);
	ConfigSection*cs= this->FindSection(Section);
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

void ConfigFile::LoadStr(const wchar * Section, const wchar * Key, wchar * Return,const wchar* Default)
{
	verify(Return!=0);
	string value = this->LoadStr(Section, Key, Default);
	strcpy(Return, value.c_str());
}

string ConfigFile::LoadStr(const wchar * Section, const wchar * Key, const wchar* Default)
{
	verify(Section != 0 && strlen(Section) != 0);
	verify(Key != 0 && strlen(Key) != 0);

	if (Default == 0)
		Default = "";
	ConfigSection* cs = this->GetEntry(Section);
	ConfigEntry* ce = cs->FindEntry(Key);
	if (!ce)
	{
		cs->SetEntry(Key, Default, CEM_SAVE);
		return Default;
	}
	else
	{
		return ce->GetValue();
	}
}

s32 ConfigFile::LoadInt(const wchar * Section, const wchar * Key,s32 Default)
{
	wchar temp_d[30];
	wchar temp_o[30];
	sprintf(temp_d,"%d",Default);
	this->LoadStr(Section,Key,temp_o,temp_d);
	if (strstr(temp_o, "0x") != NULL)
	{
		return strtol(temp_o, NULL, 16);
	}
	else
	{
		return atoi(temp_o);
	}
}
