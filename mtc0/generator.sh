#!/bin/bash

# generator.sh
# Program used to generate generated.h
# 
# Copyright 2013 Akash Rawal
# This file is part of MTC.
# 
# MTC is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# MTC is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with MTC.  If not, see <http://www.gnu.org/licenses/>.
#

#Load 
. ./config.sh

{
	#Write top comment
	echo "/*This file is autogenerated, edit generator.sh instead.*/"
	echo
	
	#For each type in uint16_t, uint32_t, uint64_t ...
	{
		echo "2 16 01        $uint16_endian"
		echo "4 32 0123      $uint32_endian"
		echo "8 64 01234567  $uint64_endian"
	} | while read size_bytes size_bits le_fmt endian
	do
		echo "/*uint${size_bits}_t endianness*/"
		
		if test "$endian" = "$le_fmt"; then
			echo "#define MTC_UINT${size_bits}_LITTLE_ENDIAN 1"
		fi
		
		for i in `seq 0 $(($size_bytes - 1))`; do
			def="MTC_UINT${size_bits}_BYTE_${i}_SIGNIFICANCE"
			echo "#define $def ${endian:$i:1}"
		done
		
		echo
	done
	
	echo "/*2's complement*/"
	if test "$int_2comp" = "yes"; then
		echo "#define MTC_INT_2_COMPLEMENT 1"
	fi
	echo
	
	#Generate signature, a little endian compile time constant
	sig_byte0="4d"
	sig_byte1="54"
	sig_byte2="43"
	sig_byte3="00"
	sig_part0=""
	sig_part1=""
	sig_part2=""
	sig_part3=""
	
	test "${uint32_endian:0:1}" = "0" && sig_part0="${sig_byte0}"
	test "${uint32_endian:0:1}" = "1" && sig_part1="${sig_byte0}"
	test "${uint32_endian:0:1}" = "2" && sig_part2="${sig_byte0}"
	test "${uint32_endian:0:1}" = "3" && sig_part3="${sig_byte0}"
	
	test "${uint32_endian:1:1}" = "0" && sig_part0="${sig_byte1}"
	test "${uint32_endian:1:1}" = "1" && sig_part1="${sig_byte1}"
	test "${uint32_endian:1:1}" = "2" && sig_part2="${sig_byte1}"
	test "${uint32_endian:1:1}" = "3" && sig_part3="${sig_byte1}"
	
	test "${uint32_endian:2:1}" = "0" && sig_part0="${sig_byte2}"
	test "${uint32_endian:2:1}" = "1" && sig_part1="${sig_byte2}"
	test "${uint32_endian:2:1}" = "2" && sig_part2="${sig_byte2}"
	test "${uint32_endian:2:1}" = "3" && sig_part3="${sig_byte2}"
	
	test "${uint32_endian:3:1}" = "0" && sig_part0="${sig_byte3}"
	test "${uint32_endian:3:1}" = "1" && sig_part1="${sig_byte3}"
	test "${uint32_endian:3:1}" = "2" && sig_part2="${sig_byte3}"
	test "${uint32_endian:3:1}" = "3" && sig_part3="${sig_byte3}"
	
	sig="${sig_part3}${sig_part2}${sig_part1}${sig_part0}"
	
	echo "/*Signature for protocol version 0*/"
	echo "#define MTC_HEADER_SIGNATURE 0x${sig}"
	echo
	
	
	echo "/*MDL include directory*/"
	echo "#define MTC_MDL_DIR \"$datadir/mtc0/mdl/\""
	echo
	
} > "./generated.h"
