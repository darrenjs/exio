## Makefile.am -- Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#XLOGHOME=/home/darrens/work/dev/src/c++/xlog/install

# NOTE: AM_CPPFLAGS is ignored in preference to a per-executable (or
# per-library) _CPPFLAGS variable if it is defined
#
# NOTE: it is not good practice to add "-Wall" and "-03" etc here.  Those
# choices should be made by the user when they invoke the configure script.
#AM_CPPFLAGS = -I$(top_srcdir)/libcpp11  -Wall -g3 -ggdb  -I$(XLOGHOME)/include
AM_CPPFLAGS = -I$(top_srcdir)/libcpp11  -Wall -g3 -ggdb


AM_LDFLAGS=-lpthread

# Primary defination of our shared library to be created.  Notice the
# _LTLIBRARIES key: automake is looking specifically for this pattern and then
# generating make rules which will invoke libtool.
#
# The "lib_" prefix indicates that the named products are to be installed in
# the $(libdir) directory. If we want to build a library that does not get
# installed, then we should use the prefix "noinst_".
lib_LTLIBRARIES = libexio.la

# The "include_" prefix includes a list of headers to be installed.  The
# "nobase_" additional prefix means the directory names are copied too.
nobase_include_HEADERS = exio/AdminInterface.h exio/sam.h exio/AdminCommand.h exio/MsgIDs.h exio/AdminSessionID.h exio/AppSvc.h exio/AdminSession.h exio/AdminIOListener.h exio/ClientCallback.h

# List the sources for an individual library
libexio_la_SOURCES = AdminCommand.cc AdminInterface.cc AdminServerSocket.cc		\
AdminSession.cc sam.cc utils.cc TableSerialiser.cc TableEvents.cc	\
Table.cc Monitor.cc AppSvc.cc AdminInterfaceImpl.cc SamBuffer.cc Reactor.cc		\
Client.cc ReactorReadBuffer.cc

# Include compile and link flags for an individual library.
#
# Note that to link against a static library which is part of this project, we
# should refers to them with relative paths inside the build directory, for
# example, "../common/libcommon.la"
#
# Note also, because we are using libtool, we should only refer to other
# libtool libraries (*.la)
#
# <library_LIBADD> : extras libtool objects to add to library
#
#libexio_la_CPPFLAGS =
libexio_la_LIBADD  = ../libcpp11/libcpp11.la -lpthread

# Note: version info for shared libraries is three number system, where the
# numbers represent: CURRENT : REVISION : AGE
#
# - CURRENT:  library version. Only change if API has changed.
# - REVISION: revision of current version. Incr. base on internal changes.
# - AGE: indicates compatibilty with previous version of the shlib.
#
#libexio_la_LDFLAGS =  -version-info 1:7:0 -L$(XLOGHOME)/lib -lxlog
libexio_la_LDFLAGS =  -version-info 2:0:0