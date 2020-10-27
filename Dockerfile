FROM alpine:3.12.1

WORKDIR /src
COPY . .

RUN apk add --virtual build-dependencies build-base git perl
# for linux/limits.h
RUN apk add --no-cache linux-headers musl-dev
RUN make clean && make -j
