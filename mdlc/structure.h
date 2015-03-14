/* structure.h
 * Dealing with structures
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

//Writes C base type for the type, ignoring complexity
void mtc_gen_base_type(MtcType type, FILE *output);

//Writes C variable for given variable
void mtc_var_gen(MtcSymbolVar *var, FILE *output);

//Writes code to count size of a given list of variables
void mtc_var_list_code_for_count
	(MtcSymbolVar *list, const char *prefix, FILE *c_file);
	
//Writes code to serialize a given list of variables
void mtc_var_list_code_for_write
	(MtcSymbolVar *list, const char *prefix, FILE *c_file);

//Writes code to deserialize a given list of variables: main code
void mtc_var_list_code_for_read
	(MtcSymbolVar *list, const char *prefix, FILE *c_file);

//Writes code to deserialize a given list of variables: 
//error handling part
void mtc_var_list_code_for_read_fail
	(MtcSymbolVar *list, const char *prefix, FILE *c_file);

//Writes code to free a given list of variables
void mtc_var_list_code_for_free
	(MtcSymbolVar *list, const char *prefix, FILE *c_file);
	
//Writes C code for given structure
void mtc_struct_gen_code
	(MtcSymbolStruct *value, FILE *h_file, FILE *c_file);
