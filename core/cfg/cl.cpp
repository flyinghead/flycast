/*
	Command line parsing
*/

#include <cstdio>
#include <cstring>

#include "cfg/cfg.h"
#include "stdclass.h"

static int setconfig(char *arg[], int cl)
{
	int rv=0;

	if (cl < 1)
	{
		WARN_LOG(COMMON, "-config : invalid number of parameters, format is section:key=value,section:key=value,...");
		return rv;
	}

	std::string value(arg[1]);
	for(;;)
	{		
		auto seppos = value.find(':');
		if (seppos == std::string::npos)
		{
			WARN_LOG(COMMON, "-config : invalid parameter %s, format is section:key=value,section:key=value,...", value.c_str());
			break;
		}
		auto eqpos = value.find('=', seppos);
		if (eqpos == std::string::npos)
		{
			WARN_LOG(COMMON, "-config : invalid parameter %s, format is section:key=value,section:key=value,...", value.c_str());
			break;
		}

		auto commapos = value.find(',', eqpos);

		std::string sect = trim_ws(value.substr(0, seppos));
		std::string key = trim_ws(value.substr(seppos + 1, eqpos - seppos - 1));
		auto endofcurrent = (commapos == std::string::npos) ? value.size() : commapos;
		std::string next = (commapos == std::string::npos) ? "" : trim_ws(value.substr(endofcurrent + 1, value.size() - endofcurrent));
		value = trim_ws(value.substr(eqpos + 1, endofcurrent - eqpos - 1));

		if (sect.empty() || key.empty())
		{
			WARN_LOG(COMMON, "-config : invalid parameter, format is section:key=value,section:key=value,...");
			break;
		}

		INFO_LOG(COMMON, "Virtual cfg %s:%s=%s", sect.c_str(), key.c_str(), value.c_str());

		cfgSetVirtual(sect, key, value);

		if (commapos == std::string::npos)
			rv++;

		if (commapos != std::string::npos)
		{
			value = next;
			continue;
		}
		else if (cl>=3 && strcmp(arg[2],",")==0)
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

void ParseCommandLine(int argc,char* argv[])
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
		else if ((*arg)[0] == '-')
		{
			WARN_LOG(COMMON, "Ignoring unknown command line option '%s'", *arg);
		}
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
}
