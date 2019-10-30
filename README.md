# ptkl - Pseudo Terminal Key Logger

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
