#!/bin/sh

aclocal $ACLOCAL_FLAGS && automake --add-missing --foreign && autoconf
