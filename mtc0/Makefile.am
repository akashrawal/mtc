
lib_LTLIBRARIES = libmtc0.la

#Generated file(s)
generated.h: generator.sh
	$(srcdir)/generator.sh
	
CLEANFILES = generated.h
BUILT_SOURCES = generated.h

#libmtc0.la
mtc_c_files =  \
	utils.c        \
	types.c        \
	serialize.c    \
	message.c      \
	io.c           \
	link.c         
#	hl_base.c      \
#	afl.c          \
#	da.c      

mtc_h_files =  \
	mtc.h          \
	common.h       \
	utils.h        \
	types.h        \
	serialize.h    \
	message.h      \
	io.h           \
	link.h         
#	hl_base.h      \
#	afl.h          \
#	da.h   
     
libmtc0_la_SOURCES = $(mtc_c_files) $(mtc_h_files)       
	        
nodist_libmtc0_la_SOURCES = generated.h
libmtc0_la_CFLAGS = -Wall -I$(top_builddir) -I$(top_srcdir)
libmtc0_la_LDFLAGS = -lm

#Headers
mtcincludedir = $(includedir)/mtc0
mtcinclude_HEADERS = $(mtc_h_files)                
                        

nodist_mtcinclude_HEADERS = generated.h    

#Distribute the generator and configuration file
EXTRA_DIST = config.sh.in generator.sh
