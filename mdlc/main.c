/* main.c
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

#include <string.h>
#include <argp.h>

#include <config.h>

//TODO: This file needs a lots of work. 

MtcMacroDB *macro_db;
int debug = 0;
const char *version_str = PACKAGE_VERSION;

error_t parser(int key, char *arg, struct argp_state *state)
{
	if (key == 'D')
	{
		int i;
		for (i = 0; arg[i] && arg[i] != '='; i++)
			;
		
		if (arg[i] != '=')
			mtc_error("Argument not in form of name=value");
		
		arg[i] = '\0';
		mtc_macro_db_add_string(macro_db, arg, arg + i + 1);
		arg[i] = '=';
		
		return 0;
	}
	if (key == 'X')
	{
		debug = 1;
		return 0;
	}
	
	return ARGP_ERR_UNKNOWN;
}

int main(int argc, char *argv[])
{
	FILE *token_out, *mpp_out, *mdlc_out, *c_file, *h_file;
	int file_len;
	char *file = NULL;
	char *buffer;
	MtcSource *source;
	MtcSymbolDB *symbol_db;
	MtcSourceMsgList *el;
	
	//Initialize macro database
	macro_db = mtc_macro_db_new();
	
	//Arguments
	{
		int arg_index;
		struct argp_option options[] = {
			{"define", 'D', "name=value", 0, 
				"Define a string macro with a given value.", 0},
			{"debug", 'X', NULL, OPTION_HIDDEN, 
				"Enable debugging", 0},
			{0}};
		struct argp argp = {
			options,
			parser,
			"input_file.mdl",
			NULL, NULL, NULL, NULL};
		
		argp_program_version = version_str;
		
		if (argp_parse(&argp, argc, argv, 0, &arg_index, NULL) != 0)
			mtc_error("Error while reading arguments");
		
		if (arg_index + 1 != argc)
			mtc_error("Exactly one file name is expected.");
		
		file = argv[arg_index];
	}
	
	file_len = strlen(file);
	
	if (file_len <= 4 || strcmp(file + file_len - 4, ".mdl") != 0)
	{
		fprintf(stderr, "Illegal file name\n");
		return 1;
	}
	
	//Open input and debug files
	source = mtc_source_new_from_file(file);
	
	if (debug)
	{
#define OPENFILE(file, ext) file = fopen(#file ext, "w")
		OPENFILE(token_out, ".txt");
		OPENFILE(mpp_out, ".txt");
		OPENFILE(mdlc_out, ".txt");
	}
	
	if (!source || (debug && (!token_out || !mpp_out || !mdlc_out)))
	{
		fprintf(stderr, "Error opening files\n");
		return 1;
	}
	
	//Initialize symbol database
	symbol_db = mtc_symbol_db_new();
	el = mtc_source_msg_list_new();
	
	{
		MtcToken *tokens = NULL;
		int n_tokens;
		MtcTokenIter iter;
		
		int fail_mode = 1;
		mtc_token_iter_init(&iter, NULL, 0);
		
		//Tokenize the file
		n_tokens = mtc_scanner_scan(source, &tokens, el);
		if (debug)
			mtc_token_list_dump(tokens, token_out);
		if (el->error_count)
			goto fail;
		
		//Run preprocessor
		mtc_token_iter_init(&iter, tokens, n_tokens);
		mtc_preprocessor_run(&iter, macro_db, el);
		if (debug)
			mtc_token_list_dump(iter.start, mpp_out);
		if (el->error_count)
			goto fail;
		
		//Run parser
		mtc_mdl_parser_parse(&iter, symbol_db, macro_db, 0, el);
		if (debug)
			mtc_symbol_db_dump(symbol_db, mdlc_out);
		if (el->error_count)
			goto fail;
		
		mtc_token_list_free(iter.start);
		fail_mode = 0;
		
	fail:
		
		//Spit out errors
		mtc_source_msg_list_dump(el, stderr);
		if (fail_mode)
			abort();
	}
	
	//Open output files
	{
		char *filename = file;
		char *i;
		size_t filename_len;
		
		//TODO: Not everyone has windowless houses like mine
		for (i = filename; *i; i++)
			if (*i == '/')
				filename = i + 1;
		filename_len = file_len - (filename - file);
		
		buffer = mtc_alloc(filename_len + 10);
		strcpy(buffer, filename);
		strcpy(buffer + filename_len - 4, "_defines.h");
		c_file = fopen(buffer, "w");
		strcpy(buffer + filename_len - 4, "_declares.h");
		h_file = fopen(buffer, "w");
		mtc_free(buffer);
	}
	
	//Generate code
	{
		MtcSymbol *iter;
		for (iter = symbol_db->head; iter; iter = iter->next)
		{
			if (! iter->isref)
			{
				if (iter->gc == mtc_symbol_struct_gc)
				{
					mtc_struct_gen_code
						((MtcSymbolStruct *) iter, h_file, c_file);
				}
				else if (iter->gc == mtc_symbol_class_gc)
				{
					mtc_class_gen_code
						((MtcSymbolClass *) iter, h_file, c_file);
				}
			}
		}
	}
	
	//Close files
	if (debug)
	{
		fclose(token_out);
		fclose(mpp_out);
		fclose(mdlc_out);
	}
	fclose(c_file);
	fclose(h_file);
	
	//Free all resources
	mtc_source_unref(source);
	mtc_macro_db_free(macro_db);
	mtc_symbol_db_free(symbol_db);
	mtc_source_msg_list_free(el);
	
	return 0;
}

