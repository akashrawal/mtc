
lib_LTLIBRARIES = libmtc0.la

#Generated file(s)
generated.h: generator.sh
	$(srcdir)/generator.sh
	
CLEANFILES = generated.h
BUILT_SOURCES = generated.h

#libmtc0.la
mtc_c =  \
	utils.c        \
	types.c        \
	serialize.c    \
	message.c      \
	event.c        \
	link.c         \
	hl_base.c      \
	afl.c          \
	router.c  

mtc_h =  \
	mtc.h          \
	common.h       \
	utils.h        \
	types.h        \
	serialize.h    \
	message.h      \
	event.h        \
	link.h         \
	hl_base.h      \
	afl.h          \
	router.h
     
libmtc0_la_SOURCES = $(mtc_c) $(mtc_h)       
	        
nodist_libmtc0_la_SOURCES = generated.h
libmtc0_la_CFLAGS = -Wall -I$(top_builddir) -I$(top_srcdir)
libmtc0_la_LDFLAGS = -lm

#Headers
mtcincludedir = $(includedir)/mtc0
mtcinclude_HEADERS = $(mtc_h)                
                        

nodist_mtcinclude_HEADERS = generated.h    

#Distribute the generator and configuration file
EXTRA_DIST = config.sh.in generator.sh
