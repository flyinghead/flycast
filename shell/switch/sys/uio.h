#ifndef SYS_UIO_H_
#define SYS_UIO_H_
#include <sys/types.h>
#include <sys/_iovec.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Read data from file descriptor FD, and put the result in the
   buffers described by IOVEC, which is a vector of COUNT 'struct iovec's.
   The buffers are filled in the order specified.
   Operates just like 'read' (see <unistd.h>) except that data are
   put in IOVEC instead of a contiguous buffer. */
extern ssize_t readv (int __fd, const struct iovec *__iovec, int __count);

/* Write data pointed by the buffers described by IOVEC, which
   is a vector of COUNT 'struct iovec's, to file descriptor FD.
   The data is written in the order specified.
   Operates just like 'write' (see <unistd.h>) except that the data
   are taken from IOVEC instead of a contiguous buffer. */
extern ssize_t writev (int __fd, const struct iovec *__iovec, int __count);

#ifdef	__cplusplus
}
#endif

#endif /* SYS_UIO_H_ */
