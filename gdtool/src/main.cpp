/*


*/

#include "types.h"
#include "imgread/common.h"
#include "cdromfs.h"

int msgboxf(const wchar* text,unsigned int type,...) {
	return MBX_OK;
}


void data_head() {
	printf("\n---\nrv=\n");
}

void data_tail() {
	printf("\n===\n");
}

void data_string(const char* s) {
	data_head();
	puts(s);
	data_tail();
}

void data_kvp(FILE* w, const char* k, const char* v, bool first) {
	fprintf(w, "%s\t\"%s\":\"%s\"", first?"\n":",\n", k, v);
}

void parse_ip_meta(FILE* w, u8* ip_meta, bool first) 
{
	char temp[256];

	const char* chm = (const char*)ip_meta;

	#define HH(ofs, len, name, first) { strncpy(temp, chm+ofs, len); temp[len]=0; for (int i=len-1; i>=0; i--) if(temp[i]==' ') temp[i]=0; else break; data_kvp(w, #name, temp, first);  }
	
	fprintf(w, "%s\"meta-info\": {\n", first?"\n":",\n");
	
	//data_kvp("type", "\"meta-info\"");

	HH(0x00,  16, hardwareId, true);
	HH(0x10,  16, makerId, false);
	
	HH(0x80, 128, productName, false);
	HH(0x4A,   6, productVersion, false);
	HH(0x50,  16, releaseDate, false);
	HH(0x40,  10, productId, false);
	HH(0x20,  16, discId, false);
	

	HH(0x30,   8, areas, false);
	HH(0x38,   8, peripherals, false);
	
	HH(0x60,  16, bootfile, false);
	HH(0x70,  16, publisher, false);

	fprintf(w, "\n}");
}


struct gdio : cdimage {
	Disc* disc;

	int pos;
	int offs;

	void seek(int to) {
		pos = to;
	}

	int read(void* to, int count) {
		u8 tmp[2048];

		verify(count != 0);
		int rem = count;

		while (rem >= 2048) {
			disc->ReadSectors( (pos/2048) + offs, 1, tmp, 2048);

			int toread = min(2048, rem) - pos%2048;

			memcpy(to, &tmp[pos%2048], toread),
			
			(char*&)to += rem;
			pos += toread;
			rem -= toread;
		}

		return count;
	}

	gdio(Disc* d) {
		pos = 0;
		offs = 150; //FAD vs LBN
		disc = d;
	}
};


bool extract_info(FILE* w, bool first, Disc* d) {

	int start_fad = -1;
	if (d->type == GdRom) {
		printf("This is a gdrom image\n");
		start_fad = d->tracks[2].StartFAD;
	}
	else {
		printf("This is a cdrom image\n");

		if (d->sessions.size() < 2) {
			printf("Not a selfboot cd\n");
			return false;
		}
		start_fad = d->sessions[1].StartFAD; 
	}


	u8 ip_meta[2048];
	d->ReadSectors(start_fad,1,ip_meta,2048);

	data_head();
	fprintf(w, first?"\n{":",\n{");
	{
		data_kvp(w, "type",d->type == GdRom? "gdrom" : "cdrom", true);

		parse_ip_meta(w, ip_meta, false);

		if (d->type == GdRom) {
			verify(d->sessions.size() == 2);
		}
			
		fprintf(w,",\n\"tracks\": [");
		{
			for (int i=0; i<d->tracks.size(); i++) {
				fprintf(w, "%s{ \"startFAD\":%d, \"endFAD\":%d, \"ctrl\":%d, \"addr\":%d }", i ==0 ? "\n":",\n", d->tracks[i].StartFAD, d->tracks[i].EndFAD, d->tracks[i].CTRL, d->tracks[i].ADDR);
			}
		}
		fprintf(w,"\n]");


		fprintf(w,",\n\"sessions\": [");
		{
			for (int i=0; i<d->sessions.size(); i++) {
				fprintf(w, "%s{ \"firstTrack\":%d, \"startFAD\":%d }", i ==0 ? "\n":",\n", d->sessions[i].FirstTrack, d->sessions[i].StartFAD);
			}
		}
		fprintf(w,"\n]");

		//im
		gdio* image = new gdio(d);
		
		if (d->type == GdRom) {
			parse_cdfs(w, image, "gdfs", d->tracks[0].StartFAD-image->offs, false);
		}

		parse_cdfs(w, image, "cdfs", start_fad-image->offs, false);

		delete image;
	}
	fprintf(w, "\n}");
	data_tail();
}
void extract_data(FILE* w, bool first, Disc* d, int start_fad, int size) {
	
	u8 temp[2048];

	int current = start_fad;

	while(size>0) {
		d->ReadSectors(current++, 1, temp, 2048);

		int step = min(2048, size);

		fwrite(temp, 1, step, w);
						
		size-=step;
	}
}

bool jsarray;
vector<string> cmds;
vector<string> files;
void exec_cmd(FILE* w, bool first,  Disc* d) {
	
	for (auto i=cmds.begin();i!=cmds.end();i++)
	{
		switch((*i)[0]) 
		{
			case 'g':
				{

				int start_fad = atoi((++i)->c_str());
				int size = atoi((++i)->c_str());

				extract_data(w, first, d, start_fad, size);
				
			}
			break;

		case 'i':
			extract_info(w, first, d);
			break;
		}
	}
}

bool process_image(FILE* w, bool& first, const string& image) {
	Disc* d = OpenDisc(image.c_str());

	if (!d) {
		printf("Invalid image, %s\n", image.c_str());
		return false;
	}

	exec_cmd(w, first, d);

	delete d;

	first = false;

	return true;
}



void parse_args(int argc, char** argv) {
	
	int i = 1;

	int rem = argc - i -1;

	while(i  < argc) {
		char** p= argv + i;

		if (p[0][0] == '-') 
		{
			i++;

			switch(p[0][1]) {
			
			case 'G':
			case 'g':
				cmds.push_back("g");
				cmds.push_back(p[1]);
				cmds.push_back(p[2]);
				i+=2;
				break;
			
			case 'I':
			case 'i':
				cmds.push_back("i");
				break;

			default:
				printf("Unknown option %c\n",*p[1]);
			}

		}
		else
			files.push_back(p[0]);

		i++;
	}

	if (cmds.size() == 0)
		cmds.push_back("i");
}

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
	FILE* w=stderr;
	bool first = true;

	parse_args(argc, argv);

	if (files.size() !=1 )
		jsarray = true;


	if (jsarray) {
		fprintf(w, "\n[\n");	
	}

	if (files.size() > 0) {
		for(auto i = files.begin(); i!= files.end(); i++) {
			process_image(w, first, *i);
		}
	}
	else {
		while (!cin.eof()) {
			string f;
			getline(cin, f);

			process_image(w, first, f);
		}
	}

	if (jsarray) {
		fprintf(w, "\n]\n");
	}
}
