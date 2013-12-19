#ifndef __CDI_H__
#define __CDI_H__



/* Basic structures */

typedef struct image_s
       {
       long               header_offset;
       long               header_position;
       long               length;
       unsigned long      version;
       unsigned short int sessions;
       unsigned short int tracks;
       unsigned short int remaining_sessions;
       unsigned short int remaining_tracks;
       unsigned short int global_current_session;
       } image_s;

typedef struct track_s
       {
       unsigned short int global_current_track;
       unsigned short int number;
       long               position;
       unsigned long      mode;
       unsigned long      sector_size;
       unsigned long      sector_size_value;
       long               length;
       long               pregap_length;
       long               total_length;
       unsigned long      start_lba;
       unsigned char      filename_length;
       } track_s;


#define CDI_V2  0x80000004
#define CDI_V3  0x80000005
#define CDI_V35 0x80000006

unsigned long ask_type(FILE *fsource, long header_position);
void CDI_init (FILE *fsource, image_s *image, char *fsourcename);
void CDI_get_sessions (FILE *fsource, image_s *image);
void CDI_get_tracks (FILE *fsource, image_s *image);
void CDI_read_track (FILE *fsource, image_s *image, track_s *track);
void CDI_skip_next_session (FILE *fsource, image_s *image);

#endif

