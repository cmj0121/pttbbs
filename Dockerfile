FROM archlinux:20200407 AS builder

RUN useradd -s /bin/bash -d /home/bbs bbsadm
RUN useradd -s /home/bbs/bin/bbsrf -d /home/bbs bbs
RUN mkdir /home/bbs
RUN chown bbs:bbs -R /home/bbs
RUN chmod 700 /home/bbs


WORKDIR /home/bbs/bbsptt
COPY . .
