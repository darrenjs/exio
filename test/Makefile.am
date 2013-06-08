


# NOTE: it is not good practice to add "-Wall" and "-03" etc here.  Those
# choices should be made by the user when they invoke the configure script.
AM_CPPFLAGS = -I$(top_srcdir)/libcpp11 -I$(top_srcdir)/libexio -Wall

AM_LDFLAGS=-pthread

LDADD = -L../libexio -lexio $(LIBLS)

noinst_PROGRAMS=slow_consumer
#noinst_PROGRAMS=server_demo

# slow_consumer

slow_consumer_SOURCES=slow_consumer.cc

# server_dem
#server_demo_SOURCES=server_demo.cc