


# NOTE: it is not good practice to add "-Wall" and "-03" etc here.  Those
# choices should be made by the user when they invoke the configure script.
AM_CPPFLAGS = -I$(top_srcdir)/libcpp11 -I$(top_srcdir)/libexio -Wall

AM_LDFLAGS=-pthread

LDADD = -L../libexio -lexio $(LIBLS)

bin_PROGRAMS=admin
#noinst_PROGRAMS=server_demo

# admin

admin_SOURCES=admin.cc

# server_dem
#server_demo_SOURCES=server_demo.cc