/* mdl.c
 * Parser for MTC Definition language
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

#include "common.h"
#include <ctype.h>
#include <errno.h>

//Convenience function for setting errors for unexpected/unrecognized word
static void mtc_expect_error
	(MtcSourceMsgList *el, MtcTokenIter *iter, const char *expected)
{
	if (iter->cur)
	{
		if (expected)
			mtc_source_msg_list_add(el, iter->cur->location, 
					MTC_SOURCE_MSG_ERROR, "%s was expected here",
					expected);
		else
			mtc_source_msg_list_add(el, iter->cur->location, 
					MTC_SOURCE_MSG_ERROR, "Unexpected token");
	}
	else
	{
		if (expected)
			mtc_source_msg_list_add(el, iter->prev->location, 
					MTC_SOURCE_MSG_ERROR, "%s was expected after this",
					expected);
		else
			mtc_source_msg_list_add(el, iter->prev->location, 
					MTC_SOURCE_MSG_ERROR, "Unexpected end of file");
	}
}

//Checks whether current token exists and has a given type
static int mtc_match_type(MtcTokenIter *iter, MtcTokenType type)
{
	if (iter->cur)
	if (iter->cur->type == type)
		return 1;
		
	return 0;
}

//Checks whether current token exists and equal to given identifier
static int mtc_match_id(MtcTokenIter *iter, const char *id)
{
	if (mtc_match_type(iter, MTC_TOKEN_ID))
	if (strcmp(iter->cur->str, id) == 0)
		return 1;
		
	return 0;
}

//Checks whether current token exists and equal to given symbol
static int mtc_match_sym(MtcTokenIter *iter, char sym)
{
	if (mtc_match_type(iter, MTC_TOKEN_SYM))
	if (iter->cur->num == sym)
		return 1;
		
	return 0;
}

//Read type from source
static int mtc_mdl_read_type
	(MtcSymbolDB *symbol_db, MtcTokenIter *iter, MtcType *res, 
	MtcSourceMsgList *el)
{
	//Test for complexity
	if (mtc_match_id(iter, "seq"))
	{
		res->complexity = MTC_TYPE_SEQ;
		mtc_token_iter_next(iter);
	}
	else if (mtc_match_id(iter, "ref"))
	{
		res->complexity = MTC_TYPE_REF;
		mtc_token_iter_next(iter);
	}
	else if (mtc_match_id(iter, "array"))
	{
		//Read number of elements
		mtc_token_iter_next(iter);
		if (! mtc_match_type(iter, MTC_TOKEN_NUM))
		{
			mtc_expect_error(el, iter, "Array length");
			return -1;
		}
		res->complexity = iter->cur->num;
		if (res->complexity <= 0)
		{
			mtc_source_msg_list_add
				(el, iter->cur->location, MTC_SOURCE_MSG_ERROR, 
				"Array length must be > 0. (it is %d)",
				res->complexity);
			return -1;
		}
		mtc_token_iter_next(iter);
	}
	else
	{
		res->complexity = MTC_TYPE_NORMAL;
	}
	
	//There should be something
	if (! mtc_match_type(iter, MTC_TOKEN_ID))
	{
		mtc_expect_error(el, iter, "A type");
		return -1;
	}
	
	//Test for any of the fundamental data types
	{
		int i;
		for (i = 0; i < MTC_TYPE_FUNDAMENTAL_N; i++)
		{
			if (strcmp(iter->cur->str, 
				mtc_type_fundamental_names[i]) == 0)
			{
				mtc_token_iter_next(iter);
				res->cat = MTC_TYPE_FUNDAMENTAL;
				res->base.fid = i;
				return 0;
			}
		}
	}
	
	//Userdefined type
	{
		char *symbol_name;
		
		symbol_name = iter->cur->str;
		
		//Fetch the symbol
		res->base.symbol = mtc_symbol_list_find
			(symbol_db->head, symbol_name);
		if (! res->base.symbol)
		{
			mtc_source_msg_list_add
				(el, iter->cur->location, MTC_SOURCE_MSG_ERROR, 
				"Unidentified type \'%s\'", symbol_name);
			return -1;
		}
		
		//Check symbol type
		if (res->base.symbol->gc != mtc_symbol_struct_gc)
		{
			mtc_source_msg_list_add
				(el, iter->cur->location, MTC_SOURCE_MSG_ERROR, 
				"\'%s\' is not a data type. ", symbol_name);
			mtc_source_msg_list_add
				(el, res->base.symbol->location, MTC_SOURCE_MSG_ERROR, 
				"'%s' was defined here.", symbol_name);
			return -1;
		}
		
		//Done!	
		
		mtc_token_iter_next(iter);
		res->cat = MTC_TYPE_USERDEFINED;
		return 0;
	}
}

//Reads a valid identifier
static char *mtc_mdl_read_idfier
	(MtcTokenIter *iter, MtcSymbolDB *symbol_db, MtcSourceMsgList *el)
{
	MtcSymbol *same_name;
	char *res;
	
	//Null-checking
	if (! mtc_match_type(iter, MTC_TOKEN_ID))
	{
		mtc_expect_error(el, iter, "An identifier");
		return NULL;
	}
	
	res = iter->cur->str;
	
	//Check for duplicates
	same_name = mtc_symbol_list_find(symbol_db->head, res);
	if (same_name)
	{
		mtc_source_msg_list_add
			(el, iter->cur->location, MTC_SOURCE_MSG_ERROR, 
			"Redefinition of identifier \'%s\'", res);
		mtc_source_msg_list_add
			(el, same_name->location, MTC_SOURCE_MSG_ERROR, 
			"'%s' was defined here.", res);
		
		return NULL;
	}
	
	mtc_token_iter_next(iter);
	
	return res;
}

//read variables
static int mtc_mdl_read_vars
	(MtcSymbolDB *symbol_db, 
	MtcTokenIter *iter,  
	MtcSymbolVar **res, 
	MtcSourceMsgList *el)
{
	MtcType type;
	MtcSymbolDB vars = MTC_SYMBOL_DB_INIT;
	char *name;
	MtcSourcePtr *location;
	
	while (1)
	{
		//exit if not identifier (can be things like ')', '|', '}', ...)
		if (! mtc_match_type(iter, MTC_TOKEN_ID))
			break;
		
		//Read a type
		if (mtc_mdl_read_type(symbol_db, iter, &type, el) < 0)
			goto end_of_loop;
		
	new_var_with_same_type:
	
		//Variable name
		name = mtc_mdl_read_idfier(iter, &vars, el);
		if (! name)
			goto end_of_loop;
		
		//Record location
		location = iter->prev->location;
		
		//Add the variable
		mtc_symbol_db_append(&vars, (MtcSymbol *) mtc_symbol_var_new
			(name, location, type));
		
		//More variables with same type
		if (mtc_match_sym(iter, MTC_SC_COMMA))
		{
			//One more variable with same type.
			mtc_token_iter_next(iter);
			goto new_var_with_same_type;
		}
		else if (mtc_match_sym(iter, MTC_SC_SC))
		{
			//Next story is different
			mtc_token_iter_next(iter);
		}
		else
		{
			//That's bad
			mtc_expect_error(el, iter, "',' or ';'");
			goto end_of_loop;
		}
		
		continue;
	
	end_of_loop:
		mtc_symbol_db_free(&vars);
		return -1;
	}
	
	//Return data
	*res = (MtcSymbolVar *) vars.head;
	
	return 0;
}

//Read a structure
static MtcSymbolStruct *mtc_mdl_read_struct
	(MtcSymbolDB *symbol_db, MtcTokenIter *iter, MtcSourceMsgList *el)
{
	char *name;
	MtcSymbolStruct *res;
	MtcSymbolVar *members;
	MtcSourcePtr *location;
	
	//Word 'struct' has already been read.
	
	//Note down the location
	location = iter->prev->location;
	
	//Read identifier
	name = mtc_mdl_read_idfier(iter, symbol_db, el);
	if (! name)
		return NULL;
	
	//Next character should be '{'
	if (! mtc_match_sym(iter, MTC_SC_LC))
	{
		mtc_expect_error(el, iter, "'{'");
		return NULL;
	}
	mtc_token_iter_next(iter);
	
	//A list of members
	if (mtc_mdl_read_vars(symbol_db, iter, &members, el) < 0)
	{
		return NULL;
	}
	
	//Now we should get '}'
	if (! mtc_match_sym(iter, MTC_SC_RC))
	{
		mtc_expect_error(el, iter, "'}'");
		mtc_symbol_list_free((MtcSymbol *) members);
		return NULL;
	}
	mtc_token_iter_next(iter);
	
	//Create new symbol for struct
	res = mtc_symbol_struct_new(name, location, members);
	
	//Done
	return res;
}

//Read a class
static MtcSymbolClass *mtc_mdl_read_class
	(MtcSymbolDB *symbol_db, MtcTokenIter *iter, MtcSourceMsgList *el)
{
	char *name;
	MtcSymbolClass *res;
	MtcSymbol *parent_class = NULL;
	MtcSymbolDB funcs  = MTC_SYMBOL_DB_INIT;
	MtcSymbolDB events = MTC_SYMBOL_DB_INIT;
	MtcSourcePtr *location;
	
	//Word 'class' has already been read.
	
	//Read identifier
	name = mtc_mdl_read_idfier(iter, symbol_db, el);
	if (! name)
		return NULL;
	
	//Note down the location
	location = iter->prev->location;
	
	//Parent class?
	if (mtc_match_sym(iter, MTC_SC_COLON))
	{
		//Next word should be parent class
		mtc_token_iter_next(iter);
		
		if (! mtc_match_type(iter, MTC_TOKEN_ID))
		{
			mtc_expect_error(el, iter, "Parent class");
			return NULL;
		}
		
		//Check whether parent class exists
		parent_class = mtc_symbol_list_find
			(symbol_db->head, iter->cur->str);
		if (! parent_class)
		{
			mtc_source_msg_list_add
				(el, iter->cur->location, MTC_SOURCE_MSG_ERROR, 
				"Parent class '%s' not found.", iter->cur->str);
			return NULL;
		}
		
		//Check type of the symbol found
		if (parent_class->gc != mtc_symbol_class_gc)
		{
			mtc_source_msg_list_add
				(el, iter->cur->location, MTC_SOURCE_MSG_ERROR, 
				"Symbol '%s' is not a class", iter->cur->str);
			mtc_source_msg_list_add
				(el, parent_class->location, MTC_SOURCE_MSG_ERROR, 
				"'%s' was defined here.", iter->cur->str);
			return NULL;
		}
		
		mtc_token_iter_next(iter);
	}
	
	//Next character should be '{'
	if (! mtc_match_sym(iter, MTC_SC_LC))
	{
		mtc_expect_error(el, iter, "'{'");
		return NULL;
	}
	mtc_token_iter_next(iter);
	
	//A list of functions and events
	while (1)
	{
		int is_fn;
		char *fn_name;
		MtcSymbolDB *target;
		MtcSymbolVar *in_args = NULL, *out_args = NULL;
		MtcSourcePtr *fn_location;
		
		//'func' or 'event' or '}' is expected
		if (mtc_match_id(iter, "func"))
		{
			is_fn = 1;
			target = &funcs;
		}
		else if (mtc_match_id(iter, "event"))
		{
			is_fn = 0;
			target = &events;
		}
		else if (mtc_match_sym(iter, MTC_SC_RC))
			break;
		else
		{
			mtc_expect_error(el, iter, "'func' or 'event' or '}'");
			
			goto end_of_loop;
		}
		
		//Note down the location 
		fn_location = iter->cur->location;
		
		mtc_token_iter_next(iter);
		
		//Function/event name
		fn_name = mtc_mdl_read_idfier(iter, target, el);
		if (! fn_name)
		{
			goto end_of_loop;
		}
		
		//Next we should get '('
		if (! mtc_match_sym(iter, MTC_SC_LB))
		{
			mtc_expect_error(el, iter, "'('");
			goto end_of_loop;
		}
		mtc_token_iter_next(iter);
		
		//Then [input] arguments
		if (mtc_mdl_read_vars(symbol_db, iter, &in_args, el) < 0)
		{
			goto end_of_loop;
		}
		
		//in case of function
		if (is_fn)
		{
			//If next word is '|', then we have out paramaters as well
			if (mtc_match_sym(iter, MTC_SC_PIPE))
			{
				mtc_token_iter_next(iter);
				//Out paramaters
				if (mtc_mdl_read_vars(symbol_db, iter, &out_args, el) < 0)
				{
					goto end_of_loop;
				}
			}
		}
		
		//Then ')'
		if (!mtc_match_sym(iter, MTC_SC_RB))
		{
			mtc_expect_error(el, iter, "')'");
			goto end_of_loop;
		}
		mtc_token_iter_next(iter);
		
		//Then ';'
		if (!mtc_match_sym(iter, MTC_SC_SC))
		{
			mtc_expect_error(el, iter, "';'");
			goto end_of_loop;
		}
		mtc_token_iter_next(iter);
		
		//Now add to the database
		if (is_fn)
		{
			mtc_symbol_db_append(target, 
				(MtcSymbol *) mtc_symbol_func_new
					(fn_name, fn_location, in_args, out_args));
		}
		else
		{
			mtc_symbol_db_append(target, 
				(MtcSymbol *) mtc_symbol_event_new
					(fn_name, fn_location, in_args));
		}
		
		
		continue;
		
	end_of_loop:
		mtc_symbol_db_free(&funcs);
		mtc_symbol_db_free(&events);
		mtc_symbol_list_free((MtcSymbol *) in_args);
		mtc_symbol_list_free((MtcSymbol *) out_args);
		
		return NULL;
	}
	
	mtc_token_iter_next(iter);
	
	//Create new symbol for struct
	res = mtc_symbol_class_new
		(name, location, (MtcSymbolClass *) parent_class, 
		(MtcSymbolFunc *) funcs.head, (MtcSymbolEvent *) events.head);
	
	//Done
	return res;
}

//Read a server
static MtcSymbolServer *mtc_mdl_read_server
	(MtcSymbolDB *symbol_db, MtcTokenIter *iter, MtcSourceMsgList *el)
{
	MtcSourcePtr *location;
	char *name;
	MtcSymbol *parent_server = NULL;
	MtcSymbolDB instances = MTC_SYMBOL_DB_INIT;
	
	//Word 'server' has already been read, continue
	
	//Name of server
	name = mtc_mdl_read_idfier(iter, symbol_db, el);
	if (! name)
	{
		//Error has been set
		return NULL;
	}
	
	//Note down the location
	location = iter->prev->location;
	
	//Parent server
	if (mtc_match_sym(iter, MTC_SC_COLON))
	{
		char *svr_name;
		
		mtc_token_iter_next(iter);
		
		if (! mtc_match_type(iter, MTC_TOKEN_ID))
		{
			mtc_expect_error(el, iter, "Parent server name");
			return NULL;
		}
		
		svr_name = iter->cur->str;
		
		parent_server = mtc_symbol_list_find(symbol_db->head, svr_name);
		
		if (! parent_server)
		{
			mtc_source_msg_list_add
				(el, iter->cur->location, MTC_SOURCE_MSG_ERROR, 
				"Parent server '%s' not found.", svr_name);
			return NULL;
		}
		
		if (parent_server->gc != mtc_symbol_server_gc)
		{
			mtc_source_msg_list_add
				(el, iter->cur->location, MTC_SOURCE_MSG_ERROR, 
				"Symbol '%s' is not a server. ", svr_name);
			mtc_source_msg_list_add
				(el, parent_server->location, MTC_SOURCE_MSG_ERROR, 
				"'%s' was defined here.", svr_name);
			return NULL;
		}
		
		mtc_token_iter_next(iter);
	}
	
	//Now we should get {
	if (! mtc_match_sym(iter, MTC_SC_LC))
	{
		mtc_expect_error(el, iter, "{");
		return NULL;
	}
	mtc_token_iter_next(iter);
	
	//Now a list of servers
	while (1)
	{
		MtcSymbol *type;
		char *type_name;
		char *instance_name;
		MtcSourcePtr *instance_location;
		
		//Break if found '}'
		if (mtc_match_sym(iter, MTC_SC_RC))
		{
			mtc_token_iter_next(iter);
			break;
		}
		
		//Read type
		if (! mtc_match_type(iter, MTC_TOKEN_ID))
		{
			mtc_expect_error(el, iter, "A class name or '}'");
			goto end_of_loop;
		}
		
		type_name = iter->cur->str;
		instance_location = iter->cur->location;
		
		type = mtc_symbol_list_find(symbol_db->head, type_name);
		
		if (! type)
		{
			mtc_source_msg_list_add
				(el, iter->cur->location, MTC_SOURCE_MSG_ERROR, 
				"Class '%s' not found.", type_name);
			goto end_of_loop;
		}
		
		if (type->gc != mtc_symbol_class_gc)
		{
			mtc_source_msg_list_add
				(el, iter->cur->location, MTC_SOURCE_MSG_ERROR, 
				"Symbol '%s' is not a class. ", type_name);
			mtc_source_msg_list_add
				(el, type->location, MTC_SOURCE_MSG_ERROR, 
				"'%s' was defined here.", type_name);
			goto end_of_loop;
		}
		
		mtc_token_iter_next(iter);
		
		//Read instance_name
		instance_name = mtc_mdl_read_idfier(iter, &instances, el);
		if (! instance_name)
			goto end_of_loop;
		
		//Next should be ";"
		if (! mtc_match_sym(iter, MTC_SC_SC))
		{
			mtc_expect_error(el, iter, ";");
			goto end_of_loop;
		}
		
		mtc_token_iter_next(iter);
		
		//Add instance
		mtc_symbol_db_append(&instances, (MtcSymbol *) mtc_symbol_instance_new
			(instance_name, instance_location, (MtcSymbolClass *) type));
		
		continue;
		
	end_of_loop:
		mtc_symbol_db_free(&instances);
		return NULL;
	}
	
	//Return the server
	return mtc_symbol_server_new
		(name, location, (MtcSymbolServer *) parent_server, 
			(MtcSymbolInstance *) instances.head);
}

//Reads symbols off a token list. Returns 0 on success, -1 on failure
int mtc_mdl_parser_parse
	(MtcTokenIter *iter, MtcSymbolDB *symbol_db, 
	MtcMacroDB *macro_db, int is_ref, MtcSourceMsgList *el)
{
	int res = 0;
	
	while (iter->cur)
	{
		MtcSymbol *symbol = NULL;
		
		//Check for struct
		if (mtc_match_id(iter, "struct"))
		{
			mtc_token_iter_next(iter);
			symbol = (MtcSymbol *) mtc_mdl_read_struct
				(symbol_db, iter, el);
			if (! symbol)
			{
				res = -1;
				break;
			}
		}
		//Check for class
		else if (mtc_match_id(iter, "class"))
		{
			mtc_token_iter_next(iter);
			symbol = (MtcSymbol *) mtc_mdl_read_class
				(symbol_db, iter, el);
			if (! symbol)
			{
				res = -1;
				break;
			}
		}
		//Check for server
		else if (mtc_match_id(iter, "server"))
		{
			mtc_token_iter_next(iter);
			symbol = (MtcSymbol *) mtc_mdl_read_server
				(symbol_db, iter, el);
			if (! symbol)
			{
				res = -1;
				break;
			}
		}
		//Refer other files
		else if (mtc_match_id(iter, "ref"))
		{
			char *srcname;
			MtcSource *source;
			MtcSourcePtr *ref_loc;
			
			mtc_token_iter_next(iter);
			
			//Must be string
			if (! mtc_match_type(iter, MTC_TOKEN_STR))
			{
				mtc_expect_error(el, iter, "A file name");
				res = -1;
				break;
			}
			
			//Tracking
			srcname = iter->cur->str;
			ref_loc = iter->cur->location;
			
			//Open the file
			source = mtc_source_new_from_file(srcname);
			if (! source)
			{
				mtc_source_msg_list_add
					(el, ref_loc, MTC_SOURCE_MSG_ERROR, 
					"Cannot open '%s' for reading: %s", 
					srcname, strerror(errno));
				res = -1;
				break;
			}
			
			//Read it
			res = mtc_mdl_parser_parse_source
				(source, symbol_db, macro_db, ref_loc, el);
			
			//Unref the source
			mtc_source_unref(source);
			
			mtc_token_iter_next(iter);
			
			if (res < 0)
				break;
		}
		//Garbage
		else 
		{
			mtc_expect_error(el, iter, 
				"'struct' or 'class' or 'server' or 'ref'");
			res = -1;
			break;
		}
		
		if (symbol)
		{
			symbol->isref = is_ref;
			mtc_symbol_db_append(symbol_db, symbol);
		}
	}
	
	mtc_token_iter_home(iter);
	
	return res;
}

//Reads symbols off a source. Returns 0 on success, -1 on failure
int mtc_mdl_parser_parse_source
	(MtcSource *source, MtcSymbolDB *symbol_db, 
	MtcMacroDB *macro_db, MtcSourcePtr *ref_loc,
	MtcSourceMsgList *el)
{
	MtcToken *tokens;
	int n_tokens;
	MtcTokenIter iter;
	int is_ref;
	int res;
	
	//Tokenize the source
	n_tokens = mtc_scanner_scan(source, &tokens, el);
	if (el->error_count)
	{
		mtc_source_msg_list_add
			(el, ref_loc, MTC_SOURCE_MSG_ERROR, 
			"Lexical scanner failed on '%s'", source->name);
		mtc_token_list_free(tokens);
		return -1;
	}
	
	//Add reference location if available
	if (ref_loc)
		mtc_token_list_add_ref(tokens, ref_loc);
	
	//Preprocess the source
	mtc_token_iter_init(&iter, tokens, n_tokens);
	mtc_preprocessor_run(&iter, macro_db, el);
	if (el->error_count)
	{
		mtc_source_msg_list_add
			(el, ref_loc, MTC_SOURCE_MSG_ERROR, 
			"Preprocessor failed on '%s'", source->name);
		mtc_token_list_free(tokens);
		return -1;
	}
	
	//Parse the source
	is_ref = ref_loc ? 1 : 0;
	res = mtc_mdl_parser_parse(&iter, symbol_db, macro_db, is_ref, el);
	
	//Free the tokens
	mtc_token_list_free(iter.start);
	
	return res;
}
