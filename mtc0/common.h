/* common.h
 * Common includes
 * 
 * Copyright 2013 Akash Rawal
 * This file is part of MTC.
 * 
 * MTC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * MTC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MTC.  If not, see <http://www.gnu.org/licenses/>.
 */

//Common header files
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#ifndef _MTC_PUBLIC
#include <string.h>
#include <error.h>
#include <assert.h>

//#include <config.h>
#define MTC_PRIVATE
#endif

#include <mtc0/generated.h>

#include "utils.h"
#include "types.h"
#include "serialize.h"
#include "message.h"
#include "io.h"
#include "event.h"
#include "link.h"
#include "hl_base.h"
#include "afl.h"
//#include "da.h"



