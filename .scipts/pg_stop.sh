#!/bin/sh
LANGUAGE="en_US.UTF-8"
LANG="en_US.UTF-8"
LC_ALL="en_US.UTF-8"
SCRIPT_DIRECTORY=$(dirname $0)
pg_ctl -D "$SCRIPT_DIRECTORY/../.pgdata" stop
