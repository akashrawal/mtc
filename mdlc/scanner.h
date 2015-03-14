/* scanner.h
 * Simple lexical scanner
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

//Token

typedef enum 
{
	//Identifier
	MTC_TOKEN_ID,
	//Numeric constant
	MTC_TOKEN_NUM,
	//String
	MTC_TOKEN_STR,
	//Special character(s)
	MTC_TOKEN_SYM
} MtcTokenType;

typedef struct _MtcToken MtcToken;
struct _MtcToken
{
	MtcToken *next, *prev;
	MtcTokenType type;
	int nl_hint;
	MtcSourcePtr *location;
	//if numeric data or symbol
	int64_t num; 
	//if identifier or string
	char str[1]; 
};

typedef enum 
{
	MTC_SC_EQ    =  0,
	MTC_SC_NE    =  1,
	MTC_SC_LE    =  2,
	MTC_SC_GE    =  3,
	MTC_SC_AND   =  4,
	MTC_SC_OR    =  5,
	MTC_SC_LB    =  6,
	MTC_SC_RB    =  7,
	MTC_SC_LC    =  8,
	MTC_SC_RC    =  9,
	MTC_SC_LS    = 10,
	MTC_SC_RS    = 11,
	MTC_SC_LT    = 12,
	MTC_SC_GT    = 13,
	MTC_SC_NOT   = 14,
	MTC_SC_PLUS  = 15,
	MTC_SC_MINUS = 16,
	MTC_SC_MUL   = 17,
	MTC_SC_DIV   = 18,
	MTC_SC_MOD   = 19,
	MTC_SC_COMMA = 20,
	MTC_SC_SC    = 21,
	MTC_SC_COLON = 22,
	MTC_SC_PIPE  = 23,
	
	MTC_SC_N  = 24
} MtcSC;

extern const char *mtc_sc_to_str[];
#define MTC_DECLARE_SC \
const char *mtc_sc_to_str[] = \
{ \
	"==", \
	"!=", \
	"<=", \
	">=", \
	"&&", \
	"||", \
	"(",  \
	")",  \
	"{",  \
	"}",  \
	"[",  \
	"]",  \
	"<",  \
	">",  \
	"!",  \
	"+",  \
	"-",  \
	"*",  \
	"/",  \
	"%",  \
	",",  \
	";",  \
	":",  \
	"|",  \
	NULL  \
};

void mtc_token_list_free(MtcToken *token);
MtcToken *mtc_token_list_copy(MtcToken *token, int n);
MtcToken *mtc_token_strcat(MtcToken *token1, MtcToken *token2);
MtcToken *mtc_token_new_num(MtcTokenType type, uint64_t num);
MtcToken *mtc_token_new_str(MtcTokenType type, const char *str);
void mtc_token_copy_location(MtcToken *to, MtcToken *from);
void mtc_token_list_add_ref
	(MtcToken *head, const MtcSourcePtr *location);
void mtc_token_list_dump(MtcToken *token, FILE *stream);

//Token iterator
typedef struct 
{
	MtcToken *start, *cur, *next, *prev;
	int pos, len;
} MtcTokenIter;

void mtc_token_iter_init(MtcTokenIter *self, MtcToken *start, int len);
MtcToken *mtc_token_iter_next(MtcTokenIter *self);
MtcToken *mtc_token_iter_prev(MtcTokenIter *self);
MtcToken *mtc_token_iter_home(MtcTokenIter *self);
int mtc_token_iter_del
	(MtcTokenIter *self, int n_tokens, MtcToken **removed);
void mtc_token_iter_ins_left(MtcTokenIter *self, MtcToken *head);
void mtc_token_iter_ins_right(MtcTokenIter *self, MtcToken *head);

//Scanner
int mtc_scanner_scan
	(MtcSource *source, MtcToken **res, MtcSourceMsgList *el);
