


# NOTE: it is not good practice to add "-Wall" and "-03" etc here.  Those
# choices should be made by the user when they invoke the configure script.
AM_CPPFLAGS = -I$(top_srcdir)/libcpp11 -I$(top_srcdir)/libexio -I$(top_builddir)/libexio -Wall

AM_LDFLAGS=-pthread

LDADD = -L../libexio -lexio $(LIBLS)

noinst_PROGRAMS=slow_consumer sam_tests example client_deletes_itself
#noinst_PROGRAMS=server_demo

# slow_consumer

slow_consumer_SOURCES=slow_consumer.cc

sam_tests_SOURCES=sam_tests.cc

example_SOURCES=example.cc

client_deletes_itself_SOURCES=client_deletes_itself.cc

# server_dem
#server_demo_SOURCES=server_demo.cc

