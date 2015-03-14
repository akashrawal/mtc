/* symbol.c
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
 
#define MTC_SYMBOL_C

#include "common.h"
#include <ctype.h>

#undef MTC_SYMBOL_C

//Creates a new symbol
static MtcSymbol *mtc_symbol_new
	(size_t len, const char *name, const MtcSourcePtr *location)
{
	MtcSymbol *res = (MtcSymbol *) mtc_alloc(len);
	
	res->name = mtc_strdup(name);
	res->location = mtc_source_ptr_copy(location);
	res->next = NULL;
	res->isref = 0;
	
	return res;
}

//Frees a symbol
void mtc_symbol_free(MtcSymbol *self)
{
	(*(self->gc))(self);
	
	mtc_free(self->name);
	mtc_source_ptr_free(self->location);
	
	mtc_free(self);
}

//Frees entire linked list of symbols
void mtc_symbol_list_free(MtcSymbol *symbols)
{
	MtcSymbol *iter, *next;
	
	for(iter = symbols; iter; iter = next)
	{
		next = iter->next;
		
		mtc_symbol_free(iter);
	}
}

//Searches for a symbol
MtcSymbol *mtc_symbol_list_find(MtcSymbol *list, const char *name)
{
	MtcSymbol *iter;
	
	for (iter = list; iter; iter = iter->next)
	{
		if (strcmp(iter->name, name) == 0)
			break;
	}
	
	return iter;
}

//Creates a new symbol database
MtcSymbolDB *mtc_symbol_db_new()
{
	MtcSymbolDB *self = (MtcSymbolDB *) mtc_alloc(sizeof(MtcSymbolDB));
	
	self->dynamic = 1;
	self->head = self->tail = NULL;
	
	return self;
}

//Frees the symbol database
void mtc_symbol_db_free(MtcSymbolDB *self)
{
	mtc_symbol_list_free(self->head);
	
	if (self->dynamic)
		mtc_free(self);
}

//Appends a symbol to the database
void mtc_symbol_db_append(MtcSymbolDB *self, MtcSymbol *symbol)
{
	if (self->tail)
		self->tail->next = symbol;
	else
		self->head = symbol;
	symbol->next = NULL;
	self->tail = symbol;
}

//Concatenates db1 and db2 to db1. db2 is destroyed.
void mtc_symbol_db_cat(MtcSymbolDB *db1, MtcSymbolDB *db2)
{
	if (db1->tail)
		db1->tail->next = db2->head;
	db1->tail = db2->tail;
	if (db2->dynamic)
		mtc_free(db2);
}

//Dump contents of a symbol
void mtc_symbol_dump(MtcSymbol *self, int depth, FILE *stream)
{
	int i;
	
	for (i = 0; i < depth; i++)
		fprintf(stream, "\t");
	
	fprintf(stream, "* Symbol(name = '%s', location: ",
		self->name);
	
	mtc_source_ptr_write(self->location, stream);
	
	fprintf(stream, ") ");
	
	(* (self->dump_func)) (self, depth, stream);
	
	fprintf(stream, "\n");
}

//Dumps the contents of the scanner
void mtc_symbol_db_dump(MtcSymbolDB *self, FILE *stream)
{
	MtcSymbol *iter;
	
	for (iter = self->head; iter; iter = iter->next)
		mtc_symbol_dump(iter, 0, stream);
}

//Writes string representation of type on a stream
void mtc_type_dump(MtcType type, FILE *stream)
{
	//Handle complexity
	if (type.complexity > 0)
	{
		//Array
		fprintf(stream, "array(%d) ", type.complexity);
	}
	else if (type.complexity == MTC_TYPE_SEQ)
	{
		//Sequence
		fprintf(stream, "seq ");
	}
	else if (type.complexity == MTC_TYPE_REF)
	{
		//Reference
		fprintf(stream, "ref ");
	}
	
	if (type.cat == MTC_TYPE_FUNDAMENTAL)
	{
		fprintf(stream, "%s", 
			mtc_type_fundamental_names[type.base.fid]);
	}
	else
	{
		fprintf(stream, "%s", type.base.symbol->name);
	}
}

//Calculates base size of a base type
MtcDLen mtc_base_type_calc_base_size(MtcType type)
{
	if (type.cat == MTC_TYPE_FUNDAMENTAL)
	{
		return  mtc_type_fundamental_sizes[type.base.fid];
	}
	else
	{
		return ((MtcSymbolStruct *) type.base.symbol)->base_size;
	}
}

//Checks whether base type is of constant size. 
int mtc_base_type_is_constsize(MtcType type)
{
	if (type.cat == MTC_TYPE_USERDEFINED)
	{
		return ((MtcSymbolStruct *) type.base.symbol)->constsize;
	}
	
	return mtc_type_fundamental_constsize[type.base.fid];
}

//Calculate size of a base type
MtcDLen mtc_type_calc_base_size(MtcType type)
{
	MtcDLen res;
	
	//Sequence
	if (type.complexity == MTC_TYPE_SEQ)
	{
		res.n_bytes = 4;
		res.n_blocks  = 0;
	}
	//Reference
	else if (type.complexity == MTC_TYPE_REF)
	{
		res.n_bytes = 1;
		res.n_blocks  = 0;
	}
	else
	{
		//Get base size of base type
		res = mtc_base_type_calc_base_size(type);
		
		//Array: multiply it with its length
		if (type.complexity > 0)
		{
			res.n_bytes *= type.complexity;
			res.n_blocks *= type.complexity;
		}
	}
	
	return res;
}

//Checks if the type is of constant size
int mtc_type_is_constsize(MtcType type)
{
	if (type.complexity == MTC_TYPE_SEQ 
	    || type.complexity == MTC_TYPE_REF)
	{
		return 0;
	}
	
	return mtc_base_type_is_constsize(type);
}

//Variable's garbage collector
void mtc_symbol_var_gc(MtcSymbol *symbol)
{
	//MtcSymbolVar *var = (MtcSymbolVar *) symbol;
}

//Dumps comtents of a variable
static void mtc_symbol_var_dump(MtcSymbol *symbol, int depth, FILE *stream)
{
	MtcSymbolVar *var = (MtcSymbolVar *) symbol;
	
	fprintf(stream, "Var(type = '");
	mtc_type_dump(var->type, stream);
	fprintf(stream, "')");
}

//Returns new variable
MtcSymbolVar *mtc_symbol_var_new
	(const char *name, const MtcSourcePtr *location, MtcType type)
{
	MtcSymbol *symbol;
	MtcSymbolVar *var;
	
	symbol = mtc_symbol_new(sizeof(MtcSymbolVar), name, location);
	
	symbol->gc = mtc_symbol_var_gc;
	symbol->dump_func = mtc_symbol_var_dump;
	
	var = (MtcSymbolVar *) symbol;
	var->type = type;
	
	return var;
}

//function's garbage collector 
void mtc_symbol_func_gc(MtcSymbol *symbol)
{
	MtcSymbolFunc *func = (MtcSymbolFunc *) symbol;
	
	mtc_symbol_list_free((MtcSymbol *) (func->in_args));
	mtc_symbol_list_free((MtcSymbol *) (func->out_args));
}

//Dumps contents of a function
static void mtc_symbol_func_dump(MtcSymbol *symbol, int depth, FILE *stream)
{
	MtcSymbolFunc *func = (MtcSymbolFunc *) symbol;
	MtcSymbol *iter;
	int i;
	
	fprintf(stream, "Func(in_args = {\n");
	for (iter = (MtcSymbol *) func->in_args; iter; iter = iter->next)
		mtc_symbol_dump(iter, depth + 1, stream);
	for (i = 0; i < depth; i++)
		fprintf(stream, "\t");
	fprintf(stream, "}, out_args = {\n");
	for (iter = (MtcSymbol *) func->out_args; iter; iter = iter->next)
		mtc_symbol_dump(iter, depth + 1, stream);
	for (i = 0; i < depth; i++)
		fprintf(stream, "\t");
	fprintf(stream, "})");
}

//Returns a new function.
MtcSymbolFunc *mtc_symbol_func_new
	(const char *name, const MtcSourcePtr *location,
	MtcSymbolVar *in_args, MtcSymbolVar *out_args)
{
	MtcSymbol *symbol;
	MtcSymbolFunc *func;
	MtcSymbolVar *iter;
	MtcDLen in_base_size = {0, 0}, out_base_size = {0, 0}, onesize;
	
	symbol = mtc_symbol_new(sizeof(MtcSymbolFunc), name, location);
	
	symbol->gc = mtc_symbol_func_gc;
	symbol->dump_func = mtc_symbol_func_dump;
	
	func = (MtcSymbolFunc *) symbol;
	func->in_args = in_args;
	func->out_args = out_args;
	
	for (iter = in_args; iter; 
		iter = (MtcSymbolVar *) (iter->parent.next))
	{
		onesize = mtc_type_calc_base_size(iter->type);
		in_base_size.n_bytes += onesize.n_bytes;
		in_base_size.n_blocks += onesize.n_blocks;
	}
	
	for (iter = out_args; iter; 
		iter = (MtcSymbolVar *) (iter->parent.next))
	{
		onesize = mtc_type_calc_base_size(iter->type);
		out_base_size.n_bytes += onesize.n_bytes;
		out_base_size.n_blocks += onesize.n_blocks;
	}
	
	func->in_args_base_size = in_base_size;
	func->out_args_base_size = out_base_size;
	
	return func;
}

//event's garbage collector 
void mtc_symbol_event_gc(MtcSymbol *symbol)
{
	MtcSymbolEvent *event = (MtcSymbolEvent *) symbol;
	
	mtc_symbol_list_free((MtcSymbol *) (event->args));
}

//Dumps contents of an event
static void mtc_symbol_event_dump(MtcSymbol *symbol, int depth, FILE *stream)
{
	MtcSymbolEvent *event = (MtcSymbolEvent *) symbol;
	MtcSymbol *iter;
	int i;
	
	fprintf(stream, "Event(args = {\n");
	for (iter = (MtcSymbol *) event->args; iter; iter = iter->next)
		mtc_symbol_dump(iter, depth + 1, stream);
	for (i = 0; i < depth; i++)
		fprintf(stream, "\t");
	fprintf(stream, "})");
}

//Returns a new event.
MtcSymbolEvent *mtc_symbol_event_new
	(const char *name, const MtcSourcePtr *location, MtcSymbolVar *args)
{
	MtcSymbol *symbol;
	MtcSymbolEvent *event;
	MtcSymbolVar *iter;
	MtcDLen base_size = {0, 0}, onesize;
	
	symbol = mtc_symbol_new(sizeof(MtcSymbolEvent), name, location);
	
	symbol->gc = mtc_symbol_event_gc;
	symbol->dump_func = mtc_symbol_event_dump;
	
	event = (MtcSymbolEvent *) symbol;
	event->args = args;
	
	for (iter = args; iter; 
		iter = (MtcSymbolVar *) (iter->parent.next))
	{
		onesize = mtc_type_calc_base_size(iter->type);
		base_size.n_bytes += onesize.n_bytes;
		base_size.n_blocks += onesize.n_blocks;
	}
	
	event->base_size = base_size;
	
	return event;
}

//struct's garbage collector 
void mtc_symbol_struct_gc(MtcSymbol *symbol)
{
	MtcSymbolStruct *struct_v = (MtcSymbolStruct *) symbol;
	
	mtc_symbol_list_free((MtcSymbol *) (struct_v->members));
}

//Dumps contents of a struct
static void mtc_symbol_struct_dump(MtcSymbol *symbol, int depth, FILE *stream)
{
	MtcSymbolStruct *struct_v = (MtcSymbolStruct *) symbol;
	MtcSymbol *iter;
	int i;
	
	fprintf(stream, "Struct(members = {\n");
	for (iter = (MtcSymbol *) struct_v->members; iter; iter = iter->next)
		mtc_symbol_dump(iter, depth + 1, stream);
	for (i = 0; i < depth; i++)
		fprintf(stream, "\t");
	fprintf(stream, "})");
}

//Returns a new structure.
MtcSymbolStruct *mtc_symbol_struct_new
	(const char *name, const MtcSourcePtr *location, MtcSymbolVar *members)
{
	MtcSymbol *symbol;
	MtcSymbolStruct *struct_v;
	MtcSymbolVar *iter;
	MtcDLen base_size = {0, 0}, onesize;
	int constsize = 1;
	
	symbol = mtc_symbol_new(sizeof(MtcSymbolStruct), name, location);
	
	symbol->gc = mtc_symbol_struct_gc;
	symbol->dump_func = mtc_symbol_struct_dump;
	
	struct_v = (MtcSymbolStruct *) symbol;
	struct_v->members = members;
	
	for (iter = members; iter; 
		iter = (MtcSymbolVar *) (iter->parent.next))
	{
		onesize = mtc_type_calc_base_size(iter->type);
		base_size.n_bytes += onesize.n_bytes;
		base_size.n_blocks += onesize.n_blocks;
		
		if (! mtc_type_is_constsize(iter->type))
			constsize = 0;
	}
	
	struct_v->base_size = base_size;
	struct_v->constsize = constsize;
	
	return struct_v;
}

//class' garbage collector 
void mtc_symbol_class_gc(MtcSymbol *symbol)
{
	MtcSymbolClass *class_v = (MtcSymbolClass *) symbol;
	
	mtc_symbol_list_free((MtcSymbol *) (class_v->funcs));
	mtc_symbol_list_free((MtcSymbol *) (class_v->events));
}

//Dumps contents of a class
static void mtc_symbol_class_dump(MtcSymbol *symbol, int depth, FILE *stream)
{
	MtcSymbolClass *class_v = (MtcSymbolClass *) symbol;
	MtcSymbol *iter;
	int i;
	
	fprintf(stream, "Class(parent_class = ");
	if (class_v->parent_class)
		fprintf(stream, "'%s'", class_v->parent_class->parent.name);
	else
		fprintf(stream, "NULL");
	fprintf(stream, ", funcs = {\n");
	for (iter = (MtcSymbol *) class_v->funcs; iter; iter = iter->next)
		mtc_symbol_dump(iter, depth + 1, stream);
	for (i = 0; i < depth; i++)
		fprintf(stream, "\t");
	fprintf(stream, "}, events = {\n");
	for (iter = (MtcSymbol *) class_v->events; iter; iter = iter->next)
		mtc_symbol_dump(iter, depth + 1, stream);
	for (i = 0; i < depth; i++)
		fprintf(stream, "\t");
	fprintf(stream, "})");
}

//Returns a new class.
MtcSymbolClass *mtc_symbol_class_new
	(const char *name, const MtcSourcePtr *location,
	MtcSymbolClass *parent_class, MtcSymbolFunc *funcs, MtcSymbolEvent *events)
{
	MtcSymbol *symbol;
	MtcSymbolClass *class_v;
	
	symbol = mtc_symbol_new(sizeof(MtcSymbolClass), name, location);
	
	symbol->gc = mtc_symbol_class_gc;
	symbol->dump_func = mtc_symbol_class_dump;
	
	class_v = (MtcSymbolClass *) symbol;
	class_v->parent_class = parent_class;
	class_v->funcs = funcs;
	class_v->events = events;
	
	return class_v;
}

//Instance's garbage collector
void mtc_symbol_instance_gc(MtcSymbol *symbol)
{
	//MtcSymbolInstance *instance = (MtcSymbolInstance *) symbol;
}

//Dumps the contents of an instance
static void mtc_symbol_instance_dump(MtcSymbol *symbol, int depth, FILE *stream)
{
	MtcSymbolInstance *instance = (MtcSymbolInstance *) symbol;
	
	fprintf(stream, "Instance(type = ");
	if (instance->type)
		fprintf(stream, "'%s'", instance->type->parent.name);
	else
		fprintf(stream, "NULL");
	fprintf(stream, ")");
}

//Returns a new instance
MtcSymbolInstance *mtc_symbol_instance_new
	(const char *name, const MtcSourcePtr *location, MtcSymbolClass *type)
{
	MtcSymbol *symbol;
	MtcSymbolInstance *instance;
	
	symbol = mtc_symbol_new(sizeof(MtcSymbolInstance), name, location);
	
	symbol->gc = mtc_symbol_instance_gc;
	symbol->dump_func = mtc_symbol_instance_dump;
	
	instance = (MtcSymbolInstance *) symbol;
	instance->type = type;
	
	return instance;
}

//Server's garbage collector
void mtc_symbol_server_gc(MtcSymbol *symbol)
{
	MtcSymbolServer *server = (MtcSymbolServer *) symbol;
	
	mtc_symbol_list_free((MtcSymbol *) server->instances);
}

//Dumps the contents of a server
static void mtc_symbol_server_dump(MtcSymbol *symbol, int depth, FILE *stream)
{
	MtcSymbolServer *server = (MtcSymbolServer *) symbol;
	MtcSymbol *iter;
	int i;
	
	fprintf(stream, "Server(parent_server = ");
	if (server->parent_server)
		fprintf(stream, "'%s'", server->parent_server->parent.name);
	else
		fprintf(stream, "NULL");
	fprintf(stream, ", instances = {\n");
	for (iter = (MtcSymbol *) server->instances; iter; iter = iter->next)
		mtc_symbol_dump(iter, depth + 1, stream);
	for (i = 0; i < depth; i++)
		fprintf(stream, "\t");
	fprintf(stream, "})");
}

//Returns a new server
MtcSymbolServer *mtc_symbol_server_new
	(const char *name, const MtcSourcePtr *location, 
	MtcSymbolServer *parent_server, MtcSymbolInstance *instances)
{
	MtcSymbol *symbol;
	MtcSymbolServer *server;
	
	symbol = mtc_symbol_new(sizeof(MtcSymbolServer), name, location);
	
	symbol->gc = mtc_symbol_server_gc;
	symbol->dump_func = mtc_symbol_server_dump;
	
	server = (MtcSymbolServer *) symbol;
	server->parent_server = parent_server;
	server->instances = instances;
	
	return server;
}
