#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LOG_FD 100
#define LEN(x) ( sizeof (x) / sizeof * (x) )
#define PRINTABLE(c) (c >= 0x20 && c <= 0x7e ? 1 : 0)

static struct {
	char *host;
	int port, sock;
	struct sockaddr_in saddr;
} logger;

int init_logger (const char *dest, const int port);

static struct {
	int (*open)(const char *, int);
	int (*openat)(int, const char *, int);
	int (*close)(int);

	int (*getpt)();
	int (*openpty)(int *, int *, char *, const struct termios *, const struct winsize *);
	pid_t (*forkpty)(int *, char *, const struct termios *, const struct winsize *);

	ssize_t (*write)(int, const void *, size_t);
	ssize_t (*writev)(int, const struct iovec *, int);

	pid_t (*fork)(void);

	int (*posix_openpt)(int);
} libc;

static struct {
	int fds[16];
} ctx;

static
void __attribute__((constructor))
init() {
	char *remote_logger = getenv("REMOTE_LOGGER");

	if (remote_logger != NULL) {
		char *ptr;

		logger.host = ptr = remote_logger;
		while (*ptr != ':')
			ptr++;
		*ptr = '\0';
		logger.port = atoi(++ptr);
		if(logger.port > 0) {
			dprintf(LOG_FD, "%d: host: %s port: %d\n", getpid(), logger.host, logger.port);
			if (init_logger(logger.host, logger.port) == -1) {
				dprintf(LOG_FD, "Error initializing remote logger\n");
				abort();
			}
		}
	} else
		logger.sock = -1;

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

	libc.fork = dlsym(RTLD_NEXT, "fork");
	if (!libc.fork) {
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
	for (int i = 0; i < LEN(ctx.fds); i++)
		if ( ctx.fds[i] != -1 )
			libc.close(ctx.fds[i]);
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
init_logger (const char *dest, const int port) {
	logger.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (logger.sock == -1) {
		dprintf(LOG_FD, "Error creating socket\n");
		return -1;
	}

	dup2(logger.sock, LOG_FD);

	memset(&logger.saddr, 0, sizeof(struct sockaddr_in));
	inet_aton(dest, &logger.saddr.sin_addr);
	logger.saddr.sin_port = htons(port);
	logger.saddr.sin_family = AF_INET;

	return connect(logger.sock, (const struct sockaddr *)&logger.saddr, sizeof(logger.saddr));
}

int
open(const char * name, int flags, ...) {
	int ret = libc.open(name, flags);
	if (strstr(name, "/ptmx")) {
		dprintf(LOG_FD, "%d: open: %s\n", getpid(), name);
		append_fd(ret);
	}
	return ret;
}

int
openat(int dir, const char * name, int flags, ...) {
	int ret = libc.openat(dir, name, flags);
	if (strstr(name, "/ptmx")) {
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
	dprintf(LOG_FD, "%d: getpt: master: %d\n", getpid(), fd);
	append_fd(fd);
	return fd;
}

int
openpty(int * master, int * slave, char * name, const struct termios * termp, const struct winsize * winp) {
	int ret;
	ret = libc.openpty(master, slave, name, termp, winp);
	dprintf(LOG_FD, "%d: openpty: master: %d slave: %d name: %s\n", getpid(), *master, *slave, name);
	append_fd(*master);
	return ret;
}

pid_t
forkpty(int * master, char * name, const struct termios * termp, const struct winsize * winp) {
	pid_t ret;
	ret = libc.forkpty(master, name, termp, winp);
	dprintf(LOG_FD, "%d: forkpty: pid: %u master: %d name: %s\n", getpid(), ret, *master, name);
	append_fd(*master);
	return ret;
}

int
posix_openpt(int flags) {
	int master;
	master = libc.posix_openpt(flags);
	dprintf(LOG_FD, "%d: posix_openpt: master: %d\n", getpid(), master);
	append_fd(master);
	return master;
}

ssize_t
write(int fd, const void * buf, size_t count) {
	ssize_t ret;
	ret = libc.write(fd, buf, count);
	if (ret > 0 && is_pts_fd(fd)) {
		dprintf(LOG_FD, "%d: write: fd: %d: size: %d: ", getpid(), fd, ret);
		int i = 0;
		do {
			char c = *((char *)buf + i++);
			char p[5] = {0,0,0,0,0};

			if (PRINTABLE(c)) p[0] = c;
				else sprintf(p, "0x%02x", c);

			libc.writev(LOG_FD, (const struct iovec[]){{ p, 5 }}, 1);
		}while(i < count);
		dprintf(LOG_FD, "\n");
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

pid_t
fork(void) {
	pid_t ret;
	ret = libc.fork();
	if (ret > 0) { // parent
		dprintf(LOG_FD, "%d: fork: ret: %d\n", getpid(), ret);
	}
	return ret;
}
