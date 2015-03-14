/* symbol.h
 * Representation of symbols in MDL in memory
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

//Symbol
typedef struct _MtcSymbol MtcSymbol;

typedef void (*MtcSymbolGC) (MtcSymbol *symbol);
typedef void (*MtcSymbolDumpFunc) (MtcSymbol *symbol, int depth, FILE *stream);

struct _MtcSymbol
{
	MtcSymbol *next;
	char *name;
	MtcSourcePtr *location;
	int isref;
	
	MtcSymbolGC gc;
	MtcSymbolDumpFunc dump_func;
};

//Frees a symbol
void mtc_symbol_free(MtcSymbol *symbol);

//Frees entire linked list
void mtc_symbol_list_free(MtcSymbol *symbols);

//Searches for a symbol
MtcSymbol *mtc_symbol_list_find(MtcSymbol *list, const char *name);

//Symbol database
typedef struct 
{
	int dynamic;
	MtcSymbol *head, *tail;
} MtcSymbolDB;

#define MTC_SYMBOL_DB_INIT {0, NULL, NULL}

//Creates a new symbol database
MtcSymbolDB *mtc_symbol_db_new();

//Frees the symbol database
void mtc_symbol_db_free(MtcSymbolDB *self);

//Adds a symbol
void mtc_symbol_db_append(MtcSymbolDB *self, MtcSymbol *symbol);

//Concatenates db1 and db2 to db1. db2 is destroyed.
void mtc_symbol_db_cat(MtcSymbolDB *db1, MtcSymbolDB *db2);

//Dump contents of a symbol
void mtc_symbol_dump(MtcSymbol *self, int depth, FILE *stream);

//Dumps the contents of the scanner
void mtc_symbol_db_dump(MtcSymbolDB *self, FILE *stream);

//Data types
typedef enum 
{
	MTC_TYPE_FUNDAMENTAL = 0,
	MTC_TYPE_USERDEFINED = 1
} MtcTypeCat;

typedef enum 
{
	MTC_TYPE_NORMAL = 0,
	MTC_TYPE_SEQ    = -1,
	MTC_TYPE_REF    = -2
	//and >= 1 if array, equal to array length
} MtcTypeComplexity;

typedef enum 
{
	MTC_TYPE_FUNDAMENTAL_UCHAR = 0,
	MTC_TYPE_FUNDAMENTAL_UINT16 = 1,
	MTC_TYPE_FUNDAMENTAL_UINT32 = 2,
	MTC_TYPE_FUNDAMENTAL_UINT64 = 3,
	MTC_TYPE_FUNDAMENTAL_CHAR = 4,
	MTC_TYPE_FUNDAMENTAL_INT16 = 5,
	MTC_TYPE_FUNDAMENTAL_INT32 = 6,
	MTC_TYPE_FUNDAMENTAL_INT64 = 7,
	MTC_TYPE_FUNDAMENTAL_FLT32 = 8,
	MTC_TYPE_FUNDAMENTAL_FLT64 = 9,
	MTC_TYPE_FUNDAMENTAL_STRING = 10,
	MTC_TYPE_FUNDAMENTAL_RAW = 11,
	MTC_TYPE_FUNDAMENTAL_MSG = 12,
	
	MTC_TYPE_FUNDAMENTAL_N = 13
} MtcTypeFundamentalID;

#ifdef MTC_SYMBOL_C
const char *mtc_type_fundamental_names[] = 
{
	"uchar",
	"uint16",
	"uint32",
	"uint64",
	"char",
	"int16",
	"int32",
	"int64",
	"flt32",
	"flt64",
	"string",
	"raw",
	"msg",
	NULL
};

const MtcDLen mtc_type_fundamental_sizes[] =
{
	{1, 0},
	{2, 0},
	{4, 0},
	{8, 0},
	{1, 0},
	{2, 0},
	{4, 0},
	{8, 0},
	{4, 0},
	{8, 0},
	{0, 1},
	{0, 1},
	{4, 0}
};

const int mtc_type_fundamental_constsize[] = 
{
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	0
};

#else 
extern const char *mtc_type_fundamental_names[];
extern const MtcDLen mtc_type_fundamental_sizes[];
extern const int mtc_type_fundamental_constsize[];
#endif


typedef struct 
{
	MtcTypeCat cat;
	int complexity;
	union 
	{
		MtcTypeFundamentalID fid;
		MtcSymbol *symbol;
	} base;
} MtcType;

MtcDLen mtc_type_calc_base_size(MtcType type);

int mtc_type_is_constsize(MtcType type);

MtcDLen mtc_base_type_calc_base_size(MtcType type);

int mtc_base_type_is_constsize(MtcType type);

//Variables (in structs, function arguments, event arguments)
typedef struct
{
	MtcSymbol parent;
	
	MtcType type;
}MtcSymbolVar;

//Returns new variable
MtcSymbolVar *mtc_symbol_var_new
	(const char *name, const MtcSourcePtr *location, MtcType type);

//Variable's garbage collector
void mtc_symbol_var_gc(MtcSymbol *symbol);

//Function
typedef struct
{
	MtcSymbol parent;
	
	MtcSymbolVar *in_args;
	MtcDLen in_args_base_size;
	MtcSymbolVar *out_args;
	MtcDLen out_args_base_size;
} MtcSymbolFunc;

//Returns a new function.
MtcSymbolFunc *mtc_symbol_func_new
	(const char *name, const MtcSourcePtr *location, 
	MtcSymbolVar *in_args, MtcSymbolVar *out_args);

//function's garbage collector 
void mtc_symbol_func_gc(MtcSymbol *symbol);

//Event
typedef struct
{
	MtcSymbol parent;
	
	MtcSymbolVar *args;
	MtcDLen base_size;
} MtcSymbolEvent;

//Returns a new event.
MtcSymbolEvent *mtc_symbol_event_new
	(const char *name, const MtcSourcePtr *location, MtcSymbolVar *args);

//event's garbage collector 
void mtc_symbol_event_gc(MtcSymbol *symbol);

//Structure
typedef struct 
{
	MtcSymbol parent;
	
	MtcSymbolVar *members;
	MtcDLen base_size;
	int constsize;
} MtcSymbolStruct;

//Creates a new structure
MtcSymbolStruct *mtc_symbol_struct_new
	(const char *name, const MtcSourcePtr *location, MtcSymbolVar *members);

void mtc_symbol_struct_gc(MtcSymbol *symbol);

//Class
typedef struct _MtcSymbolClass MtcSymbolClass;
struct _MtcSymbolClass
{
	MtcSymbol parent;
	
	MtcSymbolClass *parent_class;
	MtcSymbolFunc *funcs;
	MtcSymbolEvent *events;
};

//Creates a new class
MtcSymbolClass *mtc_symbol_class_new
	(const char *name, const MtcSourcePtr *location,
	MtcSymbolClass *parent_class, MtcSymbolFunc *funcs, MtcSymbolEvent *events);

void mtc_symbol_class_gc(MtcSymbol *symbol);

//Instances
typedef struct
{
	MtcSymbol parent;
	
	MtcSymbolClass *type;
} MtcSymbolInstance;

MtcSymbolInstance *mtc_symbol_instance_new
	(const char *name, const MtcSourcePtr *location, MtcSymbolClass *type);

void mtc_symbol_instance_gc(MtcSymbol *symbol);

//Server
typedef struct _MtcSymbolServer MtcSymbolServer;
struct _MtcSymbolServer
{
	MtcSymbol parent;
	
	MtcSymbolServer *parent_server;
	MtcSymbolInstance *instances;
};

MtcSymbolServer *mtc_symbol_server_new
	(const char *name, const MtcSourcePtr *location, 
	MtcSymbolServer *parent_server, MtcSymbolInstance *instances);

void mtc_symbol_server_gc(MtcSymbol *symbol);


