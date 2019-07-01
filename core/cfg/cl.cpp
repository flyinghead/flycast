/*
	Command line parsing
	~yay~

	Nothing too interesting here, really
*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "cfg/cfg.h"

wchar* trim_ws(wchar* str)
{
	if (str==0 || strlen(str)==0)
		return 0;

	while(*str)
	{
		if (!isspace(*str))
			break;
		str++;
	}

	size_t l=strlen(str);
	
	if (l==0)
		return 0;

	while(l>0)
	{
		if (!isspace(str[l-1]))
			break;
		str[l-1]=0;
		l--;
	}

	if (l==0)
		return 0;

	return str;
}

int setconfig(wchar** arg,int cl)
{
	int rv=0;
	for(;;)
	{
		if (cl<1)
		{
			WARN_LOG(COMMON, "-config : invalid number of parameters, format is section:key=value");
			return rv;
		}
		wchar* sep=strstr(arg[1],":");
		if (sep==0)
		{
			WARN_LOG(COMMON, "-config : invalid parameter %s, format is section:key=value", arg[1]);
			return rv;
		}
		wchar* value=strstr(sep+1,"=");
		if (value==0)
		{
			WARN_LOG(COMMON, "-config : invalid parameter %s, format is section:key=value", arg[1]);
			return rv;
		}

		*sep++=0;
		*value++=0;

		wchar* sect=trim_ws(arg[1]);
		wchar* key=trim_ws(sep);
		value=trim_ws(value);

		if (sect==0 || key==0)
		{
			WARN_LOG(COMMON, "-config : invalid parameter, format is section:key=value");
			return rv;
		}

		const wchar* constval = value;
		if (constval==0)
			constval="";
		INFO_LOG(COMMON, "Virtual cfg %s:%s=%s", sect, key, value);

		cfgSetVirtual(sect,key,value);
		rv++;

		if (cl>=3 && stricmp(arg[2],",")==0)
		{
			cl-=2;
			arg+=2;
			rv++;
			continue;
		}
		else
			break;
	}
	return rv;
}

int showhelp(wchar** arg,int cl)
{
	NOTICE_LOG(COMMON, "Available commands:");

	NOTICE_LOG(COMMON, "-config	section:key=value [, ..]: add a virtual config value\n Virtual config values won't be saved to the .cfg file\n unless a different value is written to em\nNote :\n You can specify many settings in the xx:yy=zz , gg:hh=jj , ...\n format.The spaces between the values and ',' are needed.");
	NOTICE_LOG(COMMON, "-help: show help info");

	return 0;
}

bool ParseCommandLine(int argc,wchar* argv[])
{
	cfgSetVirtual("config", "image", "");
	int cl=argc-2;
	wchar** arg=argv+1;
	while(cl>=0)
	{
		if (stricmp(*arg,"-help")==0 || stricmp(*arg,"--help")==0)
		{
			showhelp(arg,cl);
			return true;
		}
		else if (stricmp(*arg,"-config")==0 || stricmp(*arg,"--config")==0)
		{
			int as=setconfig(arg,cl);
			cl-=as;
			arg+=as;
		}
		else
		{
			char* extension = strrchr(*arg, '.');

			if (extension
				&& (stricmp(extension, ".cdi") == 0 || stricmp(extension, ".chd") == 0
					|| stricmp(extension, ".gdi") == 0 || stricmp(extension, ".lst") == 0
					|| stricmp(extension, ".cue") == 0))
			{
				INFO_LOG(COMMON, "Using '%s' as cd image", *arg);
				cfgSetVirtual("config", "image", *arg);
			}
			else if (extension && stricmp(extension, ".elf") == 0)
			{
				INFO_LOG(COMMON, "Using '%s' as reios elf file", *arg);
				cfgSetVirtual("config", "reios.enabled", "1");
				cfgSetVirtual("reios", "ElfFile", *arg);
			}
			else
			{
#if DC_PLATFORM == DC_PLATFORM_NAOMI || DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
				INFO_LOG(COMMON, "Using '%s' as rom", *arg);
				cfgSetVirtual("config", "image", *arg);
#else
				WARN_LOG(COMMON, "wtf %s is supposed to do ?",*arg);
#endif
			}
		}
		arg++;
		cl--;
	}
	return false;
}
