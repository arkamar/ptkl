#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>

#define LOG_FD 100
#define LEN(x) ( sizeof (x) / sizeof * (x) )

static struct {
	int (*open)(const char *, int);
	int (*openat)(int, const char *, int);
	int (*close)(int);

	int (*getpt)();
	int (*openpty)(int *, int *, char *, const struct termios *, const struct winsize *);
	pid_t (*forkpty)(int *, char *, const struct termios *, const struct winsize *);

	ssize_t (*write)(int, const void *, size_t);
	ssize_t (*writev)(int, const struct iovec *, int);

	int (*posix_openpt)(int);
} libc;

static struct {
	int fds[16];
} ctx;

static
void __attribute__((constructor))
init() {
	libc.open = dlsym(RTLD_NEXT, "open");
	if (!libc.open) {
		dprintf(LOG_FD, "Error in dlsym: %s\n", dlerror());
	}

	libc.openat = dlsym(RTLD_NEXT, "openat");
	if (!libc.openat) {
		dprintf(LOG_FD, "Error in dlsym: %s\n", dlerror());
	}

	libc.close = dlsym(RTLD_NEXT, "close");
	if (!libc.close) {
		dprintf(LOG_FD, "Error in dlsym: %s\n", dlerror());
	}

	libc.getpt = dlsym(RTLD_NEXT, "getpt");
	if (!libc.getpt) {
		dprintf(LOG_FD, "Error in dlsym: %s\n", dlerror());
	}

	libc.openpty = dlsym(RTLD_NEXT, "openpty");
	if (!libc.openpty) {
		dprintf(LOG_FD, "Error in dlsym: %s\n", dlerror());
	}

	libc.forkpty = dlsym(RTLD_NEXT, "forkpty");
	if (!libc.forkpty) {
		dprintf(LOG_FD, "Error in dlsym: %s\n", dlerror());
	}

	libc.write = dlsym(RTLD_NEXT, "write");
	if (!libc.write) {
		dprintf(LOG_FD, "Error in dlsym: %s\n", dlerror());
	}

	libc.writev = dlsym(RTLD_NEXT, "writev");
	if (!libc.writev) {
		dprintf(LOG_FD, "Error in dlsym: %s\n", dlerror());
	}

	libc.posix_openpt = dlsym(RTLD_NEXT, "posix_openpt");
	if (!libc.posix_openpt) {
		dprintf(LOG_FD, "Error in dlsym: %s\n", dlerror());
	}

	for (int i = 0; i < LEN(ctx.fds); i++) {
		ctx.fds[i] = -1;
	}

}

static
void __attribute__((destructor))
fini() {
}

static
void
append_fd(const int fd) {
	for (int i = 0; i < LEN(ctx.fds); i++) {
		if (ctx.fds[i] == -1) {
			ctx.fds[i] = fd;
			break;
		}
	}
}

static
void
remove_fd(const int fd) {
	for (int i = 0; i < LEN(ctx.fds); i++) {
		if (fd == ctx.fds[i]) {
			ctx.fds[i] = -1;
		}
	}
}

static
int
is_pts_fd(const int fd) {
	for (int i = 0; i < LEN(ctx.fds); i++) {
		if (fd == ctx.fds[i]) {
			return 1;
		}
	}
	return 0;
}

int
open(const char * name, int flags, ...) {
	int ret = libc.open(name, flags);
	if (ret != -1 && strstr(name, "/ptmx")) {
		dprintf(LOG_FD, "%d: open: %s\n", getpid(), name);
		append_fd(ret);
	}
	return ret;
}

int
openat(int dir, const char * name, int flags, ...) {
	int ret = libc.openat(dir, name, flags);
	if (ret != -1 && strstr(name, "/ptmx")) {
		dprintf(LOG_FD, "%d: openat: %s\n", getpid(), name);
		append_fd(ret);
	}
	return ret;
}

int
close(int fd) {
	if (fd == LOG_FD) {
		return 0;
	}
	remove_fd(fd);
	return libc.close(fd);

}

int
getpt() {
	int fd = libc.getpt();
	if (fd != -1) {
		dprintf(LOG_FD, "%d: getpt: master: %d\n", getpid(), fd);
		append_fd(fd);
	}
	return fd;
}

int
openpty(int * master, int * slave, char * name, const struct termios * termp, const struct winsize * winp) {
	int ret;
	ret = libc.openpty(master, slave, name, termp, winp);
	if (ret != -1) {
		dprintf(LOG_FD, "%d: openpty: master: %d slave: %d name: %s\n", getpid(), *master, *slave, name);
		append_fd(*master);
	}
	return ret;
}

pid_t
forkpty(int * master, char * name, const struct termios * termp, const struct winsize * winp) {
	pid_t ret;
	ret = libc.forkpty(master, name, termp, winp);
	if (ret != -1) {
		dprintf(LOG_FD, "%d: forkpty: pid: %u master: %d name: %s\n", getpid(), ret, *master, name);
		append_fd(*master);
	}
	return ret;
}

int
posix_openpt(int flags) {
	int master;
	master = libc.posix_openpt(flags);
	if (master != -1) {
		dprintf(LOG_FD, "%d: posix_openpt: master: %d\n", getpid(), master);
		append_fd(master);
	}
	return master;
}

ssize_t
write(int fd, const void * buf, size_t count) {
	ssize_t ret;
	ret = libc.write(fd, buf, count);
	if (ret > 0 && is_pts_fd(fd)) {
		dprintf(LOG_FD, "%d: write: fd: %d: size: %d: ", getpid(), fd, ret);
		libc.writev(LOG_FD, (const struct iovec[]){{ buf, count }, { "\n", 1 }}, 2);
	}
	return ret;
}

ssize_t
writev(int fd, const struct iovec * iov, int iovcnt) {
	ssize_t ret;
	ret = libc.writev(fd, iov, iovcnt);
	if (ret > 0 && is_pts_fd(fd)) {
		dprintf(LOG_FD, "%d: writev: fd: %d: size: %d", getpid(), fd, ret);
		libc.writev(LOG_FD, iov, iovcnt);
		libc.write(LOG_FD, "\n", 1);
	}
	return ret;
}
