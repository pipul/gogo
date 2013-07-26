/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */


// Wraping the blocking kernel system calls
// the following are the kernel system calls that actually block continued
// threads. we wrap it with the NON-BLOCK flag and then yield when what it
// needed is not ready, let other coroutine run first.
//
// maybe more, not all implemented.

#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include "task.h"

// sockfd must set the O_NONBLOCK file status flag by fcntl.
int sys_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	int ret;
 RETRY:
	ret = accept(sockfd, addr, addrlen);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}


int sys_open(const char *pathname, int flags, mode_t mode) {
	int ret;
 RETRY:
	// the open function default behavior: O_NONBLOCK
	ret = open(pathname, flags|O_NONBLOCK, mode);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}

int sys_fcntl(int fd, int cmd, ... /* arg */) {
	va_list args;
	int ret;
 RETRY:
	va_start(args, cmd);
	ret = fcntl(fd, cmd, args);
	va_end(args);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}


int sys_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
	int ret;
 RETRY:
	// A value of 0 indicates that the call timed out and no
	// file descriptors were ready. so we yield the thread.
	if ((ret = poll(fds, nfds, 0)) == 0) {
		yield();
		goto RETRY;
	}
	return ret;
}

int sys_select(int nfds, fd_set *readfds, fd_set *writefds,
	       fd_set *exceptfds, struct timeval *timeout) {
	int ret;
	struct timeval nonblock = {0, 0};
 RETRY:
	if ((ret = select(nfds, readfds,
			  writefds, exceptfds, &nonblock)) == 0) {
		yield();
		goto RETRY;
	}
	return ret;
}


ssize_t sys_read(int fd, void *buf, size_t count) {
	ssize_t ret;
 RETRY:
	ret = read(fd, buf, count);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}

ssize_t sys_write(int fd, void *buf, ssize_t count) {
	ssize_t ret;
 RETRY:
	ret = write(fd, buf, count);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}

ssize_t sys_readv(int fd, const struct iovec *iov, int iovcnt) {
	ssize_t ret;
 RETRY:
	ret = readv(fd, iov, iovcnt);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}

ssize_t sys_writev(int fd, const struct iovec *iov, int iovcnt) {
	ssize_t ret;
 RETRY:
	ret = writev(fd, iov, iovcnt);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}

ssize_t sys_preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
	ssize_t ret;
 RETRY:
	ret = preadv(fd, iov, iovcnt, offset);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}


ssize_t sys_pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
	ssize_t ret;
 RETRY:
	ret = pwritev(fd, iov, iovcnt, offset);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}

ssize_t sys_send(int sockfd, const void *buf, size_t len, int flags) {
	ssize_t ret;
 RETRY:
	ret = send(sockfd, buf, len, flags);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}

ssize_t sys_sendto(int sockfd, const void *buf, size_t len, int flags,
		   const struct sockaddr *dest_addr, socklen_t addrlen) {
	ssize_t ret;
 RETRY:
	ret = sendto(sockfd, buf, len, flags, dest_addr, addrlen);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}


ssize_t sys_sendmsg(int sockfd, const struct msghdr *msg, int flags) {
	ssize_t ret;
 RETRY:
	ret = sendmsg(sockfd, msg, flags);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}


ssize_t sys_recv(int sockfd, void *buf, size_t len, int flags) {
	ssize_t ret;
 RETRY:
	ret = recv(sockfd, buf, len, flags);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}

ssize_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
		     struct sockaddr *src_addr, socklen_t *addrlen) {
	ssize_t ret;
 RETRY:
	ret = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}


ssize_t sys_recvmsg(int sockfd, struct msghdr *msg, int flags) {
	ssize_t ret;
 RETRY:
	ret = recvmsg(sockfd, msg, flags);
	if (ret == -1 && errno == EAGAIN) {
		yield();
		goto RETRY;
	}
	return ret;
}
