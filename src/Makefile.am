# NOTE: it is not good practice to add "-Wall" and "-03" etc here.  Those
# choices should be made by the user when they invoke the configure script.
#AM_CPPFLAGS = -I$(top_srcdir)/libcpp11 -I$(top_srcdir)/libexio -Wall -I$(XLOGHOME)/include
AM_CPPFLAGS = -I$(top_srcdir)/libcpp11 -I$(top_srcdir)/libexio -I$(top_builddir)/libexio -Wall

# bin_PROGRAMS=admin admin_static
bin_PROGRAMS=admin

if USE_ADMINSTATIC
bin_PROGRAMS += admin_static
endif

admin_SOURCES=admin.cc
admin_LDADD=-L../libexio -lexio $(LIBLS)
admin_LDFLAGS=-L../libexio -pthread


# note, on earlier GCC, this may not work, so if you need to perform it
# manually, do _append_ something like -Wl,-Bstatic,-lstdc++,-Bdynamic to the
# link-line (and remove the -static-libstdc++ option)
if USE_ADMINSTATIC
bin_PROGRAMS += admin_static
admin_static_SOURCES=admin.cc
admin_static_LDADD=-L../libexio -lexio $(LIBLS)
admin_static_LDFLAGS=-L../libexio -pthread -static  -static-libstdc++
endif


