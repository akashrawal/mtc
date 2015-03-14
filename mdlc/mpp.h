/* mpp.h
 * MDL preprocessor
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


//Data structure to store macros

typedef struct _MtcMacroEntry MtcMacroEntry;
struct _MtcMacroEntry
{
	MtcMacroEntry *next;
	MtcToken *def;
	char name[1];
};

typedef struct
{
	MtcMacroEntry *macros;
} MtcMacroDB;


MtcMacroDB *mtc_macro_db_new();
void mtc_macro_db_add
	(MtcMacroDB *self, const char *name, MtcToken *def);
void mtc_macro_db_add_string
	(MtcMacroDB *self, const char *name, const char *value);
int mtc_macro_db_exists(MtcMacroDB *self, const char *name);
MtcToken *mtc_macro_db_fetch
	(MtcMacroDB *self, const char *name, MtcSourcePtr *expand_loc);
void mtc_macro_db_free(MtcMacroDB *self);

//Preprocessor
void mtc_preprocessor_run
	(MtcTokenIter *iter, MtcMacroDB *macro_db, MtcSourceMsgList *el);
