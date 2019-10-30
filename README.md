# ptkl - Pseudo Terminal Key Logger

**Beware!** This is very early stage of `LD_PRELOAD` based pseudo terminal key logger.

It overwrites `write` and `writev` libc functions and copies all data as they are to a file descriptor 100 from them for all fds pointing to `/dev/ptmx`.
File descriptor 100 is expected to be open in advance (use shell redirection `100>&2` for example).
Every logged write is prepended with PID, function name, fd number and amount of written data.
For example:
```
201: write: fd: 7: size: 1: s
```
where process with PID `201` wrote with function `write` to the file descriptor `7` one byte `s`.

---

Build with
```
make
```

Run following command to log ssh sessions.
```sh
./ptklify /usr/sbin/sshd -De 100>&2
```

Docker test cheat sheet:
```
docker build --tag=sshd .
docker run --rm --name=sshd sshd
ssh -o UserKnownHostsFile=kh root@172.17.0.2
```
