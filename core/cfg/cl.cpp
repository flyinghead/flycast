/*
	Command line parsing
	~yay~

	Nothing too interesting here, really
*/

#include <cstdio>
#include <cctype>
#include <cstring>

#include "cfg/cfg.h"

char* trim_ws(char* str)
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

int setconfig(char** arg,int cl)
{
	int rv=0;
	for(;;)
	{
		if (cl<1)
		{
			WARN_LOG(COMMON, "-config : invalid number of parameters, format is section:key=value");
			return rv;
		}
		char* sep=strstr(arg[1],":");
		if (sep==0)
		{
			WARN_LOG(COMMON, "-config : invalid parameter %s, format is section:key=value", arg[1]);
			return rv;
		}
		char* value=strstr(sep+1,"=");
		if (value==0)
		{
			WARN_LOG(COMMON, "-config : invalid parameter %s, format is section:key=value", arg[1]);
			return rv;
		}

		*sep++=0;
		*value++=0;

		char* sect=trim_ws(arg[1]);
		char* key=trim_ws(sep);
		value=trim_ws(value);

		if (sect==0 || key==0)
		{
			WARN_LOG(COMMON, "-config : invalid parameter, format is section:key=value");
			return rv;
		}

		const char* constval = value;
		if (constval==0)
			constval="";
		INFO_LOG(COMMON, "Virtual cfg %s:%s=%s", sect, key, constval);

		cfgSetVirtual(sect, key, constval);
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

static int showhelp()
{
	printf("Usage: flycast [OPTION]... [CONTENT]\n\n");
	printf("Options:\n");
	printf("-config	section:key=value     add a virtual config value;\n");
	printf("                              virtual config values won't be saved to the .cfg file\n");
	printf("                              unless a different value is written to them\n");
	printf("-help                         display this help\n");

	exit(0);
	return 0;
}

bool ParseCommandLine(int argc,char* argv[])
{
	settings.content.path.clear();
	int cl=argc-2;
	char** arg=argv+1;
	while(cl>=0)
	{
		if (stricmp(*arg,"-help")==0 || stricmp(*arg,"--help")==0)
		{
			showhelp();
		}
		else if (stricmp(*arg,"-config")==0 || stricmp(*arg,"--config")==0)
		{
			int as=setconfig(arg,cl);
			cl-=as;
			arg+=as;
		}
#if defined(__APPLE__)
		else if (!strncmp(*arg, "-NSDocumentRevisions", 20))
		{
			arg++;
			cl--;
		}
#endif
		else
		{
			char* extension = strrchr(*arg, '.');

			if (extension
				&& (stricmp(extension, ".cdi") == 0 || stricmp(extension, ".chd") == 0
					|| stricmp(extension, ".gdi") == 0 || stricmp(extension, ".cue") == 0))
			{
				INFO_LOG(COMMON, "Using '%s' as cd image", *arg);
				settings.content.path = *arg;
			}
			else if (extension && stricmp(extension, ".elf") == 0)
			{
				INFO_LOG(COMMON, "Using '%s' as reios elf file", *arg);
				cfgSetVirtual("config", "bios.UseReios", "yes");
				settings.content.path = *arg;
			}
			else
			{
				INFO_LOG(COMMON, "Using '%s' as rom", *arg);
				settings.content.path = *arg;
			}
		}
		arg++;
		cl--;
	}
	return false;
}
