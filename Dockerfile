FROM alpine AS builder
RUN apk add --no-cache alpine-sdk

WORKDIR ptkl
COPY . .
RUN make

FROM alpine
RUN apk add --no-cache openssh && mkdir -p /var/run/sshd && ssh-keygen -A
RUN echo 'root:toor' | chpasswd
RUN sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config

ADD docker-entrypoint.sh /bin
COPY --from=builder ptkl/libptkl.so /lib/libptkl.so

ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["/usr/sbin/sshd", "-De"]
