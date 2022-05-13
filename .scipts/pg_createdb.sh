#!/bin/sh
LANGUAGE="en_US.UTF-8"
LANG="en_US.UTF-8"
LC_ALL="en_US.UTF-8"
createdb -T template0 -l en_US.UTF-8 -E UTF8 -O postgres gazatu_api
