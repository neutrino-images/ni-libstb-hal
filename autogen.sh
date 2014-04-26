#!/bin/bash

cd $(dirname $0)
aclocal --force
libtoolize --force
autoconf --force
autoheader --force
automake --add-missing --force-missing --foreign
