#! /bin/sh

FIFO='/tmp/kl.fifo'
LOG='/tmp/kl.log'

rm -f "${FIFO}"
mkfifo "${FIFO}"
chmod 0666 "${FIFO}"
exec redirfd -wnb 1 "${FIFO}" redirfd -rnb 0 "${FIFO}" s6-log s10000000 t "${LOG}"
