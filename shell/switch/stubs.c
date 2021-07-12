#include <sys/types.h>
#include <sys/stat.h>

// Seems to be missing in newlib, dumb stub (file permissions is not a thing on fat32 anyways)
mode_t umask(mode_t mask)
{
    return mask;
}

