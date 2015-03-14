
lib_LTLIBRARIES = libmtc0.la

#Generated file(s)
generated.h: generator.sh
	$(srcdir)/generator.sh
	
CLEANFILES = generated.h
BUILT_SOURCES = generated.h

#libmtc0.la
libmtc0_la_SOURCES = \
	mtc.h           common.h       \
	utils.c         utils.h        \
	message.c       message.h      \
	types.c         types.h        \
	serialize.c     serialize.h    \
	io.c            io.h           \
	link.c          link.h         \
	hl_base.c       hl_base.h      \
	afl.c           afl.h          \
	da.c            da.h           
	        
nodist_libmtc0_la_SOURCES = generated.h
libmtc0_la_CFLAGS = -Wall -I$(top_builddir) -I$(top_srcdir)
libmtc0_la_LDFLAGS = -lm

#Headers
mtcincludedir = $(includedir)/mtc0
mtcinclude_HEADERS = mtc.h          \
                     common.h       \
                     utils.h        \
                     message.h      \
                     types.h        \
                     serialize.h    \
                     io.h           \
                     link.h         \
                     hl_base.h      \
                     afl.h          \
                     da.h                  
                        

nodist_mtcinclude_HEADERS = generated.h    

#Distribute the generator and configuration file
EXTRA_DIST = config.sh.in generator.sh
