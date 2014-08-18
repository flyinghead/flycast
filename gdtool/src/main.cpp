/*


*/

#include "types.h"
#include "imgread/common.h"
#include "cdromfs.h"

int msgboxf(const wchar* text,unsigned int type,...) {
	return MBX_OK;
}


void data_head() {
	printf("---\nrv=\n");
}

void data_tail() {
	printf("===\n");
}

void data_string(const char* s) {
	data_head();
	puts(s);
	data_tail();
}

void data_kvp(const char* k, const char* v) {
	printf("\t\"%s\":\"%s\",\n", k, v);
}

void parse_ip_meta(u8* ip_meta) 
{
	char temp[256];

	const char* chm = (const char*)ip_meta;

	#define HH(ofs, len, name) { strncpy(temp, chm+ofs, len); temp[len]=0; for (int i=len-1; i>=0; i--) if(temp[i]==' ') temp[i]=0; else break; data_kvp(#name, temp);  }

	data_head();
	printf("{\n");
	
	data_kvp("type", "\"meta-info\"");

	HH(0x00,  16, hardwareId);
	HH(0x10,  16, makerId);
	
	HH(0x80, 128, productName);
	HH(0x4A,   6, productVersion);
	HH(0x50,  16, releaseDate);
	HH(0x40,  10, productId);
	HH(0x20,  16, discId);
	

	HH(0x30,   8, areas);
	HH(0x38,   8, peripherals);
	
	HH(0x60,  16, bootfile);
	HH(0x70,  16, publisher);

	printf("}\n");
	
	data_tail();
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


int main(int argc, char** argv)
{
	for (int i=1; i<argc; i++) {

		Disc* d = OpenDisc(argv[i]);

		if (!d) {
			printf("Invalid image\n");
			//data_string("-1");
			continue;
		}


		int start_fad = -1;
		if (d->type == GdRom) {
			printf("This is a gdrom image\n");
			start_fad = d->tracks[2].StartFAD;
		}
		else {
			printf("This is a cdrom image\n");

			if (d->sessions.size() < 2) {
				printf("Not a selfboot cd\n");
				//data_string("-2");
				continue;
			}
			start_fad = d->sessions[1].StartFAD; 
		}

		u8 ip_meta[2048];
		d->ReadSectors(start_fad,1,ip_meta,2048);
		//d->Dump(argv[1]);

		parse_ip_meta(ip_meta);

		//im
		gdio* image = new gdio(d);

		
		if (d->type == GdRom) {
			parse_cdfs(image, d->tracks[0].StartFAD-image->offs);
		}

		parse_cdfs(image, start_fad-image->offs);

		delete image;

		delete d;
	}
}
