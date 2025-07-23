#!/bin/sh

USER_1=root
DIR=/usr/local/app/tars/tarsnode/data/XStats.GameRecordServer/bin
SOURCE=./GameRecordServer
HOST_1=10.10.10.100

rsync -vz $SOURCE $USER_1@$HOST_1:$DIR
