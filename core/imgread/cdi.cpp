#include "common.h"

#include "deps/chdpsr/cdipsr.h"

Disc* cdi_parse(const char* file)
{
	// Only try to open .cdi files
	size_t len = strlen(file);
	if (len > 4 && stricmp( &file[len - 4], ".cdi"))
		return nullptr;

	core_file* fsource=core_fopen(file);

	if (!fsource)
		return nullptr;

	image_s image = { 0 };
	track_s track = { 0 };
	if (!CDI_init(fsource, &image, file))
	{
		core_fclose(fsource);
		return nullptr;
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

		image.header_position = core_ftell(fsource);

		//printf("\nSession %d has %d track(s)\n",image.global_current_session,image.tracks);

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

				image.header_position = core_ftell(fsource);

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
				t.file = new RawTrackFile(core_fopen(file),track.position + track.pregap_length * track.sector_size,t.StartFAD,track.sector_size);

				rv->tracks.push_back(t);

				//printf("\n");

				//       if (track.pregap_length != 150) printf("Warning! This track seems to have a non-standard pregap...\n");

				if (track.length < 0)
					WARN_LOG(GDROM, "Negative track size found. You must extract image with /pregap option");

				//if (!opts.showinfo)
				{
					if (track.total_length < track.length + track.pregap_length)
					{
						WARN_LOG(GDROM, "This track seems truncated. Skipping...");
						core_fseek(fsource, track.position, SEEK_SET);
						core_fseek(fsource, track.total_length, SEEK_CUR);
						track.position = core_ftell(fsource);
					}
					else
					{
						
						//printf("Track position: %lu\n",track.position + track.pregap_length * track.sector_size);
						core_fseek(fsource, track.position, SEEK_SET);
						//     fseek(fsource, track->pregap_length * track->sector_size, SEEK_CUR);
						//     fseek(fsource, track->length * track->sector_size, SEEK_CUR);
						core_fseek(fsource, track.total_length * track.sector_size, SEEK_CUR);

						//savetrack(fsource, &image, &track, &opts, &flags);
						track.position = core_ftell(fsource);

						rv->EndFAD=track.start_lba +track.total_length;
						// Generate cuesheet entries

						//if (flags.create_cuesheet && !(track.mode == 2 && flags.do_convert))  // Do not generate input if converted (obsolete)
						//	savecuesheet(fcuesheet, &image, &track, &opts, &flags);

					}
				}

				core_fseek(fsource, image.header_position, SEEK_SET);


				// Close loops

				image.remaining_tracks--;
			}

			//if (flags.create_cuesheet) fclose(fcuesheet);
		}

		CDI_skip_next_session (fsource, &image);

		image.remaining_sessions--;
	}
	core_fclose(fsource);

	rv->type=GuessDiscType(CD_M1,CD_M2,CD_DA);

	rv->LeadOut.StartFAD=rv->EndFAD;
	rv->LeadOut.ADDR=0;
	rv->LeadOut.CTRL=0;



	return rv;
}

