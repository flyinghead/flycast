#include "common.h"
#include <ctype.h>

Disc* load_gdi(char* file)
{
	u32 iso_tc;
	Disc* disc = new Disc();
	
	//memset(&gdi_toc,0xFFFFFFFF,sizeof(gdi_toc));
	//memset(&gdi_ses,0xFFFFFFFF,sizeof(gdi_ses));
	FILE* t=fopen(file,"rb");
	if (!t)
		return 0;
	fscanf(t,"%d\r\n",&iso_tc);
	printf("\nGDI : %d tracks\n",iso_tc);

	char temp[512];
	char path[512];
	strcpy(path,file);
	size_t len=strlen(file);
	while (len>2)
	{
		if (path[len]=='\\' || path[len]=='/')
			break;
		len--;
	}
	len++;
	char* pathptr=&path[len];
	u32 TRACK=0,FADS=0,CTRL=0,SSIZE=0;
	s32 OFFSET=0;
	for (u32 i=0;i<iso_tc;i++)
	{
		//TRACK FADS CTRL SSIZE file OFFSET
		fscanf(t,"%d %d %d %d",&TRACK,&FADS,&CTRL,&SSIZE);
		//%s %d\r\n,temp,&OFFSET);
		//disc->tracks.push_back(
		while(isspace(fgetc(t))) ;
		fseek(t,-1,SEEK_CUR);
		if (fgetc(t)=='"')
		{
			char c;
			int i=0;
			while((c=fgetc(t))!='"')
				temp[i++]=c;
			temp[i]=0;
		}
		else
		{
			fseek(t,-1,SEEK_CUR);
			fscanf(t,"%s",temp);
		}

		fscanf(t,"%d\r\n",&OFFSET);
		printf("file[%d] \"%s\": FAD:%d, CTRL:%d, SSIZE:%d, OFFSET:%d\n",TRACK,temp,FADS,CTRL,SSIZE,OFFSET);
		
		

		Track t;
		t.ADDR=0;
		t.StartFAD=FADS+150;
		t.EndFAD=0;		//fill it in
		t.file=0;

		if (SSIZE!=0)
		{
			strcpy(pathptr,temp);
			t.file = new RawTrackFile(fopen(path,"rb"),OFFSET,t.StartFAD,SSIZE);	
		}
		disc->tracks.push_back(t);
	}

	disc->FillGDSession();

	return disc;
}


Disc* gdi_parse(char* file)
{
	size_t len=strlen(file);
	if (len>4)
	{
		if (stricmp( &file[len-4],".gdi")==0)
		{
			return load_gdi(file);
		}
	}
	return 0;
}

void iso_term()
{
}