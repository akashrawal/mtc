/* mpp.c
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

#include "common.h"

#include <errno.h>

//Data structure to store macros

//Initializes the data structure
MtcMacroDB *mtc_macro_db_new()
{
	MtcMacroDB *self = (MtcMacroDB *) mtc_alloc(sizeof(MtcMacroDB));
	
	self->macros = NULL;
	
	return self;
}

//Adds a macro definition
void mtc_macro_db_add
	(MtcMacroDB *self, const char *name, MtcToken *def)
{
	MtcMacroEntry *new_entry;
	
	new_entry = (MtcMacroEntry *) mtc_alloc
		(sizeof(MtcMacroEntry) + strlen(name));
	
	new_entry->next = self->macros;
	new_entry->def = def;
	strcpy(new_entry->name, name);
	
	self->macros = new_entry;
}

//Returns 1 if the macro exists in database. 0 otherwise
int mtc_macro_db_exists(MtcMacroDB *self, const char *name)
{
	MtcMacroEntry *iter;
	
	//Find the given macro
	for (iter = self->macros; iter; iter = iter->next)
	{
		if (strcmp(iter->name, name) == 0)
			return 1;
	}
	
	return 0;
}

//Fetches a copy of macro definition with location modified 
//accordingly
MtcToken *mtc_macro_db_fetch
	(MtcMacroDB *self, const char *name, MtcSourcePtr *expand_loc)
{
	MtcToken *res;
	
	{
		MtcMacroEntry *iter;
		
		//Find the given macro
		for (iter = self->macros; iter; iter = iter->next)
		{
			if (strcmp(iter->name, name) == 0)
				break;
		}
		
		if (! iter)
			return NULL;
		
		//Duplicate the token list
		res = mtc_token_list_copy(iter->def, -1);
	}
	
	//If expand_loc was specified, append it to location of each 
	//token
	if (expand_loc)
	{
		MtcToken *iter;
		
		for (iter = res; iter; iter = iter->next)
		{
			if (iter->location)
			{
				mtc_source_ptr_append_copy
					(iter->location, expand_loc, MTC_SOURCE_INV_MACRO);
			}
		}
	}
	
	//Done
	return res;
}

//Frees all memory associated with the data structure
void mtc_macro_db_free(MtcMacroDB *self)
{
	MtcMacroEntry *iter, *bak;
	
	for (iter = self->macros; iter; iter = bak)
	{
		bak = iter->next;
		
		mtc_token_list_free(iter->def);
		mtc_free(iter);
	}
	
	mtc_free(self);
}

//Runs preprocessor a given sequence of tokens
void mtc_preprocessor_run
	(MtcTokenIter *iter, MtcMacroDB *macro_db, MtcSourceMsgList *el)
{
	int last_if = -1, open_if = -1, if_level = 0, last_include = -1;
	
	while (iter->cur)
	{
		while (iter->cur)
		{
			//String concatenation
			if (iter->pos >= 1)
			if (iter->cur->type == MTC_TOKEN_STR)
			if (iter->prev->type == MTC_TOKEN_STR)
			{
				MtcToken *res = mtc_token_strcat
					(iter->prev, iter->cur);
				mtc_token_iter_del(iter, 2, NULL);
				mtc_token_iter_ins_left(iter, res);
				
				continue;
			}
			
			//Brackets: convert (x) to x
			if (iter->pos >= 2)
			if (iter->prev->prev->type == MTC_TOKEN_SYM)
			if (iter->prev->prev->num == MTC_SC_LB)
			if (iter->cur->type == MTC_TOKEN_SYM)
			if (iter->cur->num == MTC_SC_RB)
			{
				mtc_token_iter_del(iter, 1, NULL);
				mtc_token_iter_prev(iter);
				mtc_token_iter_del(iter, 1, NULL);
				mtc_token_iter_next(iter);
				
				continue;
			}
			
			//Binary operators
			if (iter->pos >= 2)
			if (iter->prev->prev->type == MTC_TOKEN_NUM)
			if (iter->cur->type == MTC_TOKEN_NUM)
			{
				//Macro will reduce typing a lot
#define _MTC_OPERATOR(mdl_form, c_form) \
				if (iter->prev->type == MTC_TOKEN_SYM) \
				if (iter->prev->num == mdl_form) \
				{ \
					iter->prev->prev->num \
						= ( iter->prev->prev->num \
						c_form iter->cur->num); \
					mtc_token_iter_del(iter, 2, NULL); \
					\
					continue;\
				} \
				if (iter->next) \
				if (iter->next->type == MTC_TOKEN_SYM) \
				if (iter->next->num == mdl_form) \
					goto blocked_by_higher_precedence;
				
				//Now we list operators in decreasing order of 
				//precedence
				_MTC_OPERATOR(MTC_SC_MUL, *)
				_MTC_OPERATOR(MTC_SC_DIV, /)
				_MTC_OPERATOR(MTC_SC_MOD, %)
				_MTC_OPERATOR(MTC_SC_PLUS, +)
				_MTC_OPERATOR(MTC_SC_MINUS, -)
				_MTC_OPERATOR(MTC_SC_LT, <)
				_MTC_OPERATOR(MTC_SC_GT, >)
				_MTC_OPERATOR(MTC_SC_LE, <=)
				_MTC_OPERATOR(MTC_SC_GE, >=)
				_MTC_OPERATOR(MTC_SC_EQ, ==)
				_MTC_OPERATOR(MTC_SC_NE, !=)
				_MTC_OPERATOR(MTC_SC_AND, &&)
				_MTC_OPERATOR(MTC_SC_OR, ||)
				
#undef _MTC_OPERATOR
				
			blocked_by_higher_precedence:
				break;
				;
			}
			
			//Unary operators
			if (iter->pos >= 1)
			if (iter->prev->type == MTC_TOKEN_SYM)
			if (iter->cur->type == MTC_TOKEN_NUM)
			{
				//Macro will reduce typing a lot
#define _MTC_OPERATOR(mdl_form, c_form) \
				if (iter->prev->num == mdl_form) \
				{ \
					iter->cur->num \
						= (c_form iter->cur->num); \
					mtc_token_iter_prev(iter); \
					mtc_token_iter_del(iter, 1, NULL); \
					mtc_token_iter_next(iter); \
					\
					continue;\
				} 
				
				//Now we list operators
				_MTC_OPERATOR(MTC_SC_PLUS, +)
				_MTC_OPERATOR(MTC_SC_MINUS, -)
				_MTC_OPERATOR(MTC_SC_NOT, !)
				
#undef _MTC_OPERATOR
				
			}
			
			//_defined operator
			if (iter->pos >= 1)
			if (iter->prev->type == MTC_TOKEN_ID)
			if (strcmp(iter->prev->str, "_defined") == 0)
			if (iter->cur->type == MTC_TOKEN_ID)
			{
				MtcToken *res = mtc_token_new_num(MTC_TOKEN_NUM, 
					mtc_macro_db_exists(macro_db, iter->cur->str));
				mtc_token_copy_location(res, iter->cur);
				mtc_token_iter_del(iter, 2, NULL);
				mtc_token_iter_ins_left(iter, res);
				
				continue;
			}
			
			//Macro expansion
			if (iter->cur->type == MTC_TOKEN_ID)
			{
				MtcToken *expansion;
				if ((expansion = mtc_macro_db_fetch
					(macro_db, iter->cur->str, iter->cur->location)))
				{
					mtc_token_iter_del(iter, 1, NULL);
					mtc_token_iter_ins_right(iter, expansion);	
					
					continue;
				}
			}
			
			//Defining a macro
			if (iter->cur->type == MTC_TOKEN_ID)
			if (strcmp(iter->cur->str, "_define") == 0)
			{
				int pos_bak;
				const char *macro_name;
				MtcToken *macro_def;
				
				//Assertion
				if (! iter->cur->nl_hint)
				{
					mtc_source_msg_list_add
						(el, iter->cur->location, MTC_SOURCE_MSG_ERROR,
						"\'_define\' is not first token of the line"); 
				}
				
				mtc_token_iter_next(iter);
				
				//Macro name
				if ((! iter->cur)
					|| (iter->cur->type != MTC_TOKEN_ID)
					|| (iter->cur->nl_hint != 0))
				{
					mtc_source_msg_list_add
						(el, iter->prev->location, MTC_SOURCE_MSG_ERROR,
						"Macro name expected after \'_define\'"); 
					
					break;
				}
				
				macro_name = iter->cur->str;
				
				//Move iterator to end of the line
				pos_bak = iter->pos;
				while (iter->next && (iter->next->nl_hint == 0))
					mtc_token_iter_next(iter);
				
				//Add the macro to database
				mtc_token_iter_del
					(iter, iter->pos - pos_bak, &macro_def);
				mtc_macro_db_add(macro_db, macro_name, macro_def);
				
				//Remove the _define macro_name
				mtc_token_iter_del(iter, 2, NULL);
				
				continue;
			}
			
			//_if directive beginning
			if (iter->cur->type == MTC_TOKEN_ID)
			if (strcmp(iter->cur->str, "_if") == 0)
			{
				//Assertion
				if (! iter->cur->nl_hint)
				{
					mtc_source_msg_list_add
						(el, iter->cur->location, MTC_SOURCE_MSG_ERROR,
						"\'_if\' is not first token of the line"); 
				}
				
				last_if = iter->pos;
				if (open_if == -1)
					open_if = last_if;
			}
			
			//_if evaluation
			if (last_if >= 0)
			if (iter->next)
			if (iter->next->nl_hint)
			{
				int cond;
				
				//By now _if expression should have been evaluated
				//so it should be only 1 token wide
				//Also it can only be an integer
				if ((iter->pos - last_if != 1)
					|| (iter->cur->type != MTC_TOKEN_NUM))
				{
					mtc_source_msg_list_add
						(el, iter->cur->location, MTC_SOURCE_MSG_ERROR,
						"Invalid expression for \'_if\'"); 
					
					break;
				}
				
				//Tracking
				last_if = -1;
				if_level++;
				cond = iter->cur->num;
				
				//Eat away the _if statement
				mtc_token_iter_del(iter, 2, NULL);
				
				//If condition evaluates as false, 
				//eat away the body of _if
				if (! cond)
				{
					int false_level = if_level, n_del = 0;
					
					while (mtc_token_iter_next(iter))
					{
						n_del++;
						
						if (iter->cur->type == MTC_TOKEN_ID)
						{
							//Handle _if inside
							if (strcmp(iter->cur->str, "_if") == 0)
							{
								if_level++;
							}
							//Handle _endif
							if (strcmp(iter->cur->str, "_endif") == 0)
							{
								if_level--;
								if (if_level < false_level)
									break;
							}
							//Handle _else
							if (strcmp(iter->cur->str, "_else") == 0)
							{
								if (if_level == false_level)
									break;
							}
							//Handle _elseif by changing to _if
							if (strcmp(iter->cur->str, "_elseif") == 0)
							{
								if (if_level == false_level)
								{
									MtcToken *if_token
										= mtc_token_new_str
										(MTC_TOKEN_ID, "_if");
									mtc_token_copy_location
										(if_token, iter->cur);
									mtc_token_iter_del(iter, 1, NULL);
									mtc_token_iter_ins_right
										(iter, if_token);
									n_del--;
									if_level--;
									break;
								}
							}
						}
					}
					
					//Do the 'eating away'
					mtc_token_iter_del(iter, n_del, NULL);
				}
				
				break;
			}
			
			//If we see _else or _elseif then previous _if or _elseif 
			//has evaluated as true, so we just assume this as false
			if (iter->cur->type == MTC_TOKEN_ID)
			if ((strcmp(iter->cur->str, "_elseif") == 0)
				|| (strcmp(iter->cur->str, "_else") == 0))
			{
				int false_level = if_level, n_del = 1;
				
				//Assertion
				if (! iter->cur->nl_hint)
				{
					mtc_source_msg_list_add
						(el, iter->cur->location, MTC_SOURCE_MSG_ERROR,
						"\'_else[if]\' is not first token of the line"); 
				}
				
				//Pop error on unmatched if
				if (if_level == 0)
				{
					mtc_source_msg_list_add
						(el, iter->cur->location, MTC_SOURCE_MSG_ERROR,
						"No \'_if\' matching \'_else[if]\'"); 
					
					break;
				}
				
				//Move to the end of the if
				while (mtc_token_iter_next(iter))
				{
					n_del++;
					
					if (iter->cur->type == MTC_TOKEN_ID)
					{
						//Handle _if inside
						if (strcmp(iter->cur->str, "_if") == 0)
						{
							if_level++;
						}
						//Handle _endif
						if (strcmp(iter->cur->str, "_endif") == 0)
						{
							if_level--;
							if (if_level < false_level)
								break;
						}
					}
				}
				
				//Delete the tokens
				mtc_token_iter_del(iter, n_del, NULL);
			}
			
			//_endif
			if (iter->cur->type == MTC_TOKEN_ID)
			if (strcmp(iter->cur->str, "_endif") == 0)
			{	
				//Assertion
				if (! iter->cur->nl_hint)
				{
					mtc_source_msg_list_add
						(el, iter->cur->location, MTC_SOURCE_MSG_ERROR,
						"\'_endif\' is not first token of the line"); 
				}
				
				//Pop error on unmatched if
				if (if_level == 0)
				{
					mtc_source_msg_list_add
						(el, iter->cur->location, MTC_SOURCE_MSG_ERROR,
						"No \'_if\' matching \'_endif\'"); 
					
					break;
				}
				
				if_level--;
				
				//Delete it
				mtc_token_iter_del(iter, 1, NULL);
				
				break;
			}
			
			//_include directive beginning
			if (iter->cur->type == MTC_TOKEN_ID)
			if (strcmp(iter->cur->str, "_include") == 0)
			{
				//Assertion
				if (! iter->cur->nl_hint)
				{
					mtc_source_msg_list_add
						(el, iter->cur->location, MTC_SOURCE_MSG_ERROR,
						"\'_include\' is not first token of the line"); 
				}
				
				last_include = iter->pos;
			}
			
			//_include directive evaluation
			if (last_include >= 0)
			if (iter->next ? iter->next->nl_hint : 1)
			{
				MtcSource *incl_source;
				MtcToken *token_iter, *incl_tokens = NULL;
				
				//By now expression should have been evaluated
				//so it should be only 1 token wide
				//Also it can only be a string
				if ((iter->pos - last_include != 1)
					|| (iter->cur->type != MTC_TOKEN_STR))
				{
					mtc_source_msg_list_add
						(el, iter->cur->location, MTC_SOURCE_MSG_ERROR,
						"Invalid expression for \'_include\'"); 
					
					break;
				}
				
				last_include = -1;
				
				//Read the file
				incl_source = mtc_source_new_from_file
					(iter->cur->str);
				
				if (! incl_source)
				{
					mtc_source_msg_list_add
						(el, iter->cur->location, MTC_SOURCE_MSG_ERROR,
						"Cannot read \'%s\': %s", 
						iter->cur->str, strerror(errno));
				}
				else
				{
					//Tokenize it
					mtc_scanner_scan(incl_source, &incl_tokens, el);
					mtc_source_unref(incl_source);
				}
				if (incl_tokens)
				{	
					//Add include's location to the tokens
					for (token_iter = incl_tokens; 
						token_iter; token_iter = token_iter->next)
					{
						mtc_source_ptr_append_copy
							(token_iter->location, iter->cur->location,
							MTC_SOURCE_INV_INCLUDE);
					}
					
					//Then insert it
					mtc_token_iter_ins_right(iter, incl_tokens);
				}
				
				//Remove the _include directive
				mtc_token_iter_del(iter, 2, NULL);
				
			}
			
			break;
		}
		
		mtc_token_iter_next(iter);
	}
	
	mtc_token_iter_home(iter);
}
