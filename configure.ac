#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#TODO: update website
AC_INIT([MTC],[0.0.0],[],[mtc],[http://code.google.com/p/mtc-ipc-library])
AC_CONFIG_SRCDIR([mtc0/common.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror silent-rules])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE([disable])
AM_PROG_AR
LT_INIT

# Checks for programs.
#TODO: Enforce C11
AC_PROG_CC_C99
AM_PROG_CC_C_O

#Checks for endianess
AC_LANG_PUSH([C])

#uint16_t
AC_CACHE_CHECK([endianness of uint16_t], [mtc_cv_endian_uint16],
	[
	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdio.h>
#include <stdint.h>

int main()
{
	uint16_t num = 0x0100;
	char *p = (char *) &num;
	FILE *out = fopen("conftest.out",
	                  "w");
	
	if (! out)
		return 1;
  
	fprintf(out, "%d%d\n",
	        (int) p[0],
	        (int) p[1]);
  
	fclose(out);
  
	return 0;
}
		]])],
		[mtc_cv_endian_uint16=`cat conftest.out`],
		[AC_MSG_FAILURE([something broke while testing endianness])],
		[AC_MSG_FAILURE([cannot test endianness while cross-compiling])]
	)
	]
)

#uint32_t
AC_CACHE_CHECK([endianness of uint32_t], [mtc_cv_endian_uint32],
	[
	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdio.h>
#include <stdint.h>

int main()
{
	uint32_t num = 0x03020100L;
	char *p = (char *) &num;
	FILE *out = fopen("conftest.out",
	                  "w");
	
	if (! out)
		return 1;
  
	fprintf(out, "%d%d%d%d\n",
	        (int) p[0],
	        (int) p[1],
	        (int) p[2],
	        (int) p[3]);
  
	fclose(out);
  
	return 0;
}
		]])],
		[mtc_cv_endian_uint32=`cat conftest.out`],
		[AC_MSG_FAILURE([something broke while testing endianness])],
		[AC_MSG_FAILURE([cannot test endianness while cross-compiling])]
	)
	]
)

#uint64_t
AC_CACHE_CHECK([endianness of uint64_t], [mtc_cv_endian_uint64],
	[
	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdio.h>
#include <stdint.h>

int main()
{
	uint64_t num = 0x0706050403020100LL;
	char *p = (char *) &num;
	FILE *out = fopen("conftest.out",
	                  "w");
	
	if (! out)
		return 1;
  
	fprintf(out, "%d%d%d%d%d%d%d%d\n",
	        (int) p[0],
	        (int) p[1],
	        (int) p[2],
	        (int) p[3],
	        (int) p[4],
	        (int) p[5],
	        (int) p[6],
	        (int) p[7]);
  
	fclose(out);
  
	return 0;
}
		]])],
		[mtc_cv_endian_uint64=`cat conftest.out`],
		[AC_MSG_FAILURE([something broke while testing endianness])],
		[AC_MSG_FAILURE([cannot test endianness while cross-compiling])]
	)
	]
)

#Checks for 2's complement
AC_CACHE_CHECK(
	[whether signed integer is in 2 complement form],
	[mtc_cv_bool_2complement],
	[AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdio.h>
#include <stdint.h>

int main()
{
	int v = -1;
	
	FILE *out = fopen("conftest.out", "w");
	if (! out)
		return 1;
	
	if (! (~ ((unsigned int) v)))
		fprintf(out, "yes");
	else
		fprintf(out, "no");
	
	return 0;
}
		]])],
		[mtc_cv_bool_2complement=`cat conftest.out`],
		[AC_MSG_FAILURE([something broke while testing 2 complement])],
		[AC_MSG_FAILURE([cannot test 2 complement while cross compiling])]
	)]
)
		

AC_LANG_POP([C])

#Write all output

AC_SUBST([MTC_UINT16_ENDIAN], [$mtc_cv_endian_uint16])
AC_SUBST([MTC_UINT32_ENDIAN], [$mtc_cv_endian_uint32])
AC_SUBST([MTC_UINT64_ENDIAN], [$mtc_cv_endian_uint64])
AC_SUBST([MTC_INT_2COMP], [$mtc_cv_bool_2complement])

AC_CONFIG_FILES([Makefile
                 data/Makefile
                 data/mtc0.pc
                 mtc0/Makefile
                 mtc0/config.sh
                 mdlc/Makefile
                 doc/Makefile])
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT
