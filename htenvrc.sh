#!/usr/bin/env bash
# Copyright (C) 2000 by Massimiliano Ghilardi
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
#
# This file reads global and user profiles (like /etc/profile and ~/.profile)
# then outputs all environment variables to let twin server know them too.
#
# HedgeyTTY pty profile: if ~/.config/hedgeytty/profile says "pty", ensure
# TERM is usable for xterm/termcap (do not override TERM=linux on console).

#
# Read the default system settings.
#
test -r /etc/profile     && { . /etc/profile     > /dev/null 2>&1 ; }
test -r ${HOME}/.bashrc  && { . ${HOME}/.bashrc  > /dev/null 2>&1 ; }
test -r ${HOME}/.profile && { . ${HOME}/.profile > /dev/null 2>&1 ; }
#

# pty profile TERM fix (Termux / macOS Terminal)
_ht_prof="${HOME}/.config/hedgeytty/profile"
if [ "${HEDGEYTTY_PROFILE:-}" = "pty" ] || { [ -r "$_ht_prof" ] && [ "$(cat "$_ht_prof" 2>/dev/null)" = "pty" ]; }; then
  case "${TERM:-}" in
    ""|dumb|unknown|linux) export TERM=xterm-256color ;;
  esac
fi

exec 1>&2

printenv || env || set
