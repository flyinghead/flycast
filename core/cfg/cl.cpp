/*
	Command line parsing
	~yay~

	Nothing too interesting here, really
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

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
			printf("-config : invalid number of parameters, format is section:key=value\n");
			return rv;
		}
		wchar* sep=strstr(arg[1],":");
		if (sep==0)
		{
			printf("-config : invalid parameter %s, format is section:key=value\n",arg[1]);
			return rv;
		}
		wchar* value=strstr(sep+1,"=");
		if (value==0)
		{
			printf("-config : invalid parameter %s, format is section:key=value\n",arg[1]);
			return rv;
		}

		*sep++=0;
		*value++=0;

		wchar* sect=trim_ws(arg[1]);
		wchar* key=trim_ws(sep);
		value=trim_ws(value);

		if (sect==0 || key==0)
		{
			printf("-config : invalid parameter, format is section:key=value\n");
			return rv;
		}

		if (value==0)
			value="";
		printf("Virtual cfg %s:%s=%s\n",sect,key,value);

		cfgSetVitual(sect,key,value);
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
	printf("Available commands :\n");

	printf("-config	section:key=value [, ..]: add a virtual config value\n Virtual config values wont be saved to the .cfg file\n unless a different value is writen to em\nNote :\n You can specify many settings in the xx:yy=zz , gg:hh=jj , ...\n format.The spaces betwen the values and ',' are needed.");

	return 0;
}
bool ParseCommandLine(int argc,wchar* argv[])
{

	int cl=argc-2;
	wchar** arg=argv+1;
	while(cl>=0)
	{
		if (stricmp(*arg,"-help")==0)
		{
			int as=showhelp(arg,cl);
			cl-=as;
			arg+=as;
			return true;
		}
		else if (stricmp(*arg,"-config")==0)
		{
			int as=setconfig(arg,cl);
			cl-=as;
			arg+=as;
		}
		else
		{
			printf("wtf %s is suposed to do ?\n",*arg);
		}
		arg++;
		cl--;
	}
	printf("\n");
	return false;
}