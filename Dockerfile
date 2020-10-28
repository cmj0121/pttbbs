FROM alpine:3.12.1

# install the necessary library
RUN apk add --virtual build-dependencies build-base git perl
# for linux/limits.h
RUN apk add --no-cache linux-headers musl-dev

RUN addgroup -g 99 bbs
RUN adduser -G bbs -u 9999 -D -h /home/bbs bbs

WORKDIR /src
COPY . .
RUN chown -R bbs:bbs /src

# --- build source code ----
USER bbs
RUN make clean
RUN make -j
RUN PREFIX=/home/bbs/bin make install

# ---- init and run ----
WORKDIR /home/bbs
RUN [ -f .PASSWDS ] || ./bin/initbbs -DoIt
CMD ./bin/shmctl init && bin/mbbsd -p 23 -D
