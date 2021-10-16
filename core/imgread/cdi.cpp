#include "common.h"
#include "stdclass.h"

#include "deps/chdpsr/cdipsr.h"

Disc* cdi_parse(const char* file, std::vector<u8> *digest)
{
	if (get_file_extension(file) != "cdi")
		return nullptr;

	FILE *fsource = nowide::fopen(file, "rb");

	if (fsource == nullptr)
		throw FlycastException(std::string("Cannot open CDI file ") + file);

	image_s image = { 0 };
	track_s track = { 0 };
	if (!CDI_init(fsource, &image, file))
	{
		std::fclose(fsource);
		throw FlycastException(std::string("Invalid CDI file ") + file);
	}

	CDI_get_sessions(fsource,&image);

	Disc* rv= new Disc();

	image.remaining_sessions = image.sessions;

	/////////////////////////////////////////////////////////////// Loop sessions
	
	bool ft=true, CD_M2=false,CD_M1=false,CD_DA=false;

	while(image.remaining_sessions > 0)
	{
		ft=true;
		image.global_current_session++;

		CDI_get_tracks (fsource, &image);

		image.header_position = std::ftell(fsource);

		if (image.tracks == 0)
			INFO_LOG(GDROM, "Open session");
		else
		{
			// Clear cuesheet
			image.remaining_tracks = image.tracks;

			///////////////////////////////////////////////////////////////// Loop tracks

			while(image.remaining_tracks > 0)
			{
				track.global_current_track++;
				track.number = image.tracks - image.remaining_tracks + 1;

				CDI_read_track (fsource, &image, &track);

				image.header_position = std::ftell(fsource);

				// Show info
#if 0
				printf("Saving  ");
				printf("Track: %2d  ",track.global_current_track);
				printf("Type: ");
				switch(track.mode)
				{
				case 0 : printf("Audio/"); break;
				case 1 : printf("Mode1/"); break;
				case 2 :
				default: printf("Mode2/"); break;
				}
				printf("%lu  ",track.sector_size);
				
				printf("Pregap: %-3ld  ",track.pregap_length);
				printf("Size: %-6ld  ",track.length);
				printf("LBA: %-6ld  ",track.start_lba);
#endif
				if (ft)
				{
					ft=false;
					Session s;
					s.StartFAD=track.pregap_length + track.start_lba;
					s.FirstTrack=track.global_current_track;
					rv->sessions.push_back(s);
				}

				Track t;
				if (track.mode==2)
					CD_M2=true;
				if (track.mode==1)
					CD_M1=true;
				if (track.mode==0)
					CD_DA=true;

				t.ADDR=1;//hmm is that ok ?

				t.CTRL=track.mode==0?0:4;
				t.StartFAD=track.start_lba+track.pregap_length;
				t.EndFAD=t.StartFAD+track.length-1;
				t.file = new RawTrackFile(nowide::fopen(file, "rb"), track.position + track.pregap_length * track.sector_size, t.StartFAD, track.sector_size);

				rv->tracks.push_back(t);

				if (track.length < 0)
					WARN_LOG(GDROM, "Negative track size found. You must extract image with /pregap option");

				std::fseek(fsource, track.position, SEEK_SET);
				if (track.total_length < track.length + track.pregap_length)
				{
					WARN_LOG(GDROM, "This track seems truncated. Skipping...");
					// FIXME that can't be right
					std::fseek(fsource, track.total_length, SEEK_CUR);
				}
				else
				{
					std::fseek(fsource, track.total_length * track.sector_size, SEEK_CUR);
					rv->EndFAD=track.start_lba +track.total_length;
				}
				track.position = std::ftell(fsource);

				std::fseek(fsource, image.header_position, SEEK_SET);

				image.remaining_tracks--;
			}
		}

		CDI_skip_next_session(fsource, &image);

		image.remaining_sessions--;
	}
	if (digest != nullptr)
		*digest = MD5Sum().add(fsource).getDigest();
	std::fclose(fsource);

	rv->type=GuessDiscType(CD_M1,CD_M2,CD_DA);

	rv->LeadOut.StartFAD=rv->EndFAD;
	rv->LeadOut.ADDR=0;
	rv->LeadOut.CTRL=0;

	return rv;
}

