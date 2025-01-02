#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

// Seems to be missing in newlib, dumb stub (file permissions is not a thing on fat32 anyways)
mode_t umask(mode_t mask)
{
    return mask;
}

int pause()
{
	sleep(0xffffffff);
	return -1;
}

// FIXME always failing stub
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
  switch (how)
    {
    case SIG_BLOCK:
    case SIG_UNBLOCK:
    case SIG_SETMASK:
      break;
    default:
      errno = EINVAL;
      return -1;
    }
  errno = ENOSYS;
  return -1;
}

// Map an interface index into its name.
char *if_indextoname(unsigned ifindex, char *ifname)
{
	errno = ENXIO;
	return NULL;
}
