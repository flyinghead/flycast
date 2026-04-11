#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "cdipsr.h"

// Global variables

unsigned long temp_value;


/////////////////////////////////////////////////////////////////////////////

unsigned long ask_type(hostfs::File *fsource, long header_position)
{

unsigned char filename_length;
unsigned long track_mode;

    fsource->seek(header_position, SEEK_SET);
    fsource->read(&temp_value, 4, 1);
    if (temp_value != 0)
       fsource->seek(8, SEEK_CUR); // extra data (DJ 3.00.780 and up)
    fsource->seek(24, SEEK_CUR);
    fsource->read(&filename_length, 1, 1);
    fsource->seek(filename_length, SEEK_CUR);
    fsource->seek(19, SEEK_CUR);
    fsource->read(&temp_value, 4, 1);
       if (temp_value == 0x80000000)
          fsource->seek(8, SEEK_CUR); // DJ4
    fsource->seek(16, SEEK_CUR);
    fsource->read(&track_mode, 4, 1);
    fsource->seek(header_position, SEEK_SET);
    return (track_mode);
}


/////////////////////////////////////////////////////////////////////////////


bool CDI_read_track (hostfs::File *fsource, image_s *image, track_s *track)
{

     unsigned char TRACK_START_MARK[10] = { 0, 0, 0x01, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF };
     unsigned char current_start_mark[10];

         fsource->read(&temp_value, 4, 1);
         if (temp_value != 0)
            fsource->seek(8, SEEK_CUR); // extra data (DJ 3.00.780 and up)

         fsource->read(&current_start_mark, 10, 1);
         if (memcmp(TRACK_START_MARK, current_start_mark, 10)) {
        	 printf("CDI_read_track: Unsupported format: Could not find the track start mark\n");
        	 return false;
         }

         fsource->read(&current_start_mark, 10, 1);
         if (memcmp(TRACK_START_MARK, current_start_mark, 10)) {
        	 printf("CDI_read_track: Unsupported format: Could not find the track start mark\n");
        	 return false;
         }

         fsource->seek(4, SEEK_CUR);
         fsource->read(&track->filename_length, 1, 1);
         fsource->seek(track->filename_length, SEEK_CUR);
         fsource->seek(11, SEEK_CUR);
         fsource->seek(4, SEEK_CUR);
         fsource->seek(4, SEEK_CUR);
         fsource->read(&temp_value, 4, 1);
            if (temp_value == 0x80000000)
               fsource->seek(8, SEEK_CUR); // DJ4
         fsource->seek(2, SEEK_CUR);
         fsource->read(&track->pregap_length, 4, 1);
         fsource->read(&track->length, 4, 1);
         fsource->seek(6, SEEK_CUR);
         fsource->read(&track->mode, 4, 1);
         fsource->seek(12, SEEK_CUR);
         fsource->read(&track->start_lba, 4, 1);
         fsource->read(&track->total_length, 4, 1);
         fsource->seek(16, SEEK_CUR);
         fsource->read(&track->sector_size_value, 4, 1);

         switch(track->sector_size_value)
               {
               case 0 : track->sector_size = 2048; break;
               case 1 : track->sector_size = 2336; break;
               case 2 : track->sector_size = 2352; break;
               case 4 : track->sector_size = 2448; break;
               default:
            	   printf("CDI_read_track: Unsupported sector size. value %ld\n", track->sector_size_value);
               	   return false;
               }

         if (track->mode > 2) {
        	 printf("CDI_read_track: Unsupported format: Track mode not supported\n");
        	 return false;
         }

         fsource->seek(29, SEEK_CUR);
         if (image->version != CDI_V2)
         {
            fsource->seek(5, SEEK_CUR);
            fsource->read(&temp_value, 4, 1);
            if (temp_value == 0xffffffff)
                fsource->seek(78, SEEK_CUR); // extra data (DJ 3.00.780 and up)
         }
         return true;
}


void CDI_skip_next_session (hostfs::File *fsource, image_s *image)
{
     fsource->seek(4, SEEK_CUR);
     fsource->seek(8, SEEK_CUR);
     if (image->version != CDI_V2) fsource->seek(1, SEEK_CUR);
}

bool CDI_get_tracks (hostfs::File *fsource, image_s *image)
{
     return fsource->read(&image->tracks, 2, 1) == 1;
}

bool CDI_init (hostfs::File *fsource, image_s *image, const char *fsourcename)
{
	fsource->seek(0, SEEK_END);
	image->length = fsource->tell();

	if (image->length < 8)
	{
		printf("%s: Image file is too short\n", fsourcename);
		return false;
	}

	fsource->seek(image->length-8, SEEK_SET);
	if (fsource->read(&image->version, 4, 1) != 1
			|| fsource->read(&image->header_offset, 4, 1) != 1)
		return false;

	if ((image->version != CDI_V2 && image->version != CDI_V3 && image->version != CDI_V35)
			|| image->header_offset == 0)
	{
		printf("%s: Bad image format\n", fsourcename);
		return false;
	}
	return true;
}

bool CDI_get_sessions (hostfs::File *fsource, image_s *image)
{
#ifndef DEBUG_CDI
     if (image->version == CDI_V35)
        fsource->seek((image->length - image->header_offset), SEEK_SET);
     else
        fsource->seek(image->header_offset, SEEK_SET);

#else
     fsource->seek(0L, SEEK_SET);
#endif
     return fsource->read(&image->sessions, 2, 1) == 1;
}

