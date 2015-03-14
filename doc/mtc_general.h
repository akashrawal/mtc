/**
 * \addtogroup mtc_general
 * \{
 * MTC is a high performance general purpose IPC library.
 * 
 * \defgroup mtc_building Compiling and installing MTC itself
 * \ingroup mtc_general
 * \{
 * MTC is a cross-unix library.
 * 
 * MTC uses GNU Autotools build system. After extracting the release tarball,
 * open a terminal in the extracted directory and type:
 * ~~~~~{.sh}
 * ./configure
 * make
 * make install
 * ~~~~~
 * You can type `./configure --help` to get complete list of compile-time options.
 * 
 * Cross-compiling MTC is probably possible, but it hasn't been tested yet. 
 * You need to set cache variables:
 * 
 * | Variable                  | Description                                 |
 * |---------------------------|---------------------------------------------|
 * | mtc_cv_endian_uint16      | Endianness of 2-byte integers (see below)   |
 * | mtc_cv_endian_uint32      | Endianness of 4-byte integers (see below)   |
 * | mtc_cv_endian_uint64      | Endianness of 8-byte integers (see below)   |
 * | mtc_cv_bool_2complement   | Whether the host uses 2's complement form   |
 * |                           | for signed integers (yes/no)                |
 * 
 * Endianness of a _n_ byte integer is described as a string of _n_ digits
 * from 0 to _n_ - 1 where each digit _d_ at position _p_ from the left 
 * means that significance of byte _p_ is _d_, where 0 means least significant
 * byte.
 * 
 * e.g. on little endian systems:
 * 
 *     mtc_cv_endian_uint16=01
 *     mtc_cv_endian_uint32=0123
 *     mtc_cv_endian_uint64=01234567
 * 
 * and on big endian systems:
 * 
 *     mtc_cv_endian_uint16=10
 *     mtc_cv_endian_uint32=3210
 *     mtc_cv_endian_uint64=76543210
 * \}
 * 
 * \defgroup mtc_usage Using MTC in applications
 * \ingroup mtc_general
 * \{
 * To use MTC in your applications you have to do two things.
 * 
 * First, include <mtc0/mtc.h> header file in your sources
 * 
 * Then link against _mtc0_ package by using pkg-config utility.
 * 
 * e.g. this is how to compile a single file program:
 * ~~~~~{.sh}
 * cc prog.c \`pkg-config --libs mtc0\` -o prog
 * ~~~~~
 * 
 * \}
 * 
 * \}
 */
