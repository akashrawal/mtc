/* mdl.c
 * parser for MTC Definition language
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

//Reads symbols off a token list. Returns 0 on success, -1 on failure
int mtc_mdl_parser_parse
	(MtcTokenIter *iter, MtcSymbolDB *symbol_db, 
	MtcMacroDB *macro_db, int is_ref, MtcSourceMsgList *el);

//Reads symbols off a source. Returns 0 on success, -1 on failure
int mtc_mdl_parser_parse_source
	(MtcSource *source, MtcSymbolDB *symbol_db, 
	MtcMacroDB *macro_db, MtcSourcePtr *ref_loc,
	MtcSourceMsgList *el);
