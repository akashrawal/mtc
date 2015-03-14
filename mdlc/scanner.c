/* scanner.c
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

#include "common.h"
#include <ctype.h>

MTC_DECLARE_SC

//Extra character functions

static int mtc_isid_1(int ch)
{
	if ((ch >= 'A' && ch <= 'Z')
		|| (ch >= 'a' && ch <= 'z')
		|| (ch == '_'))
		return 1;
	else 
		return 0;
}

static int mtc_isid(int ch)
{
	if ((ch >= 'A' && ch <= 'Z')
		|| (ch >= 'a' && ch <= 'z')
		|| (ch >= '0' && ch <= '9')
		|| (ch == '_'))
		return 1;
	else 
		return 0;
}

//MtcToken

void mtc_token_list_free(MtcToken *token)
{
	MtcToken *bak;
	for (; token; token = bak)
	{
		bak = token->next;
		
		mtc_source_ptr_free(token->location);
		mtc_free(token);
	}
}

MtcToken *mtc_token_list_copy(MtcToken *token, int n)
{
	MtcToken *res = NULL, *cur;
	MtcToken **next_ptr = &res, *prev = NULL;
	
	for (; token && n; token = token->next, n--)
	{
		cur = (MtcToken *) mtc_alloc
			(sizeof(MtcToken) + strlen(token->str));
		
		*cur = *token;
		cur->location = mtc_source_ptr_copy(token->location);
		strcpy(cur->str, token->str);
		
		*next_ptr = cur;
		next_ptr = &(cur->next);
		cur->prev = prev;
		prev = cur;
	}
	
	if (res)
		cur->next = NULL;
	
	return res;
}

MtcToken *mtc_token_strcat(MtcToken *token1, MtcToken *token2)
{
	size_t len;
	MtcToken *res;
	
	//Calculate size of resultant token
	len = sizeof(MtcToken) + strlen(token1->str) + strlen(token2->str);
	
	//Create
	res = (MtcToken *) mtc_alloc(len);
	*res = *token1;
	res->next = res->prev = NULL;
	
	//Assign it a location
	res->location = mtc_source_ptr_copy(token1->location);
	mtc_source_ptr_append_copy(res->location, token2->location, 
		MTC_SOURCE_INV_CAT);
	
	//Concat data
	strcpy(res->str, token1->str);
	strcat(res->str, token2->str);
	
	//done
	return res;
}

MtcToken *mtc_token_new_num(MtcTokenType type, uint64_t num)
{
	MtcToken *res;
	
	res = (MtcToken *) mtc_alloc(sizeof(MtcToken));
	
	res->type = type; 
	res->num = num;
	res->str[0] = 0;
	res->next = res->prev = NULL;
	res->location = NULL;
	res->nl_hint = 0;
	
	return res;
}

MtcToken *mtc_token_new_str(MtcTokenType type, const char *str)
{
	MtcToken *res;
	
	res = (MtcToken *) mtc_alloc
		(sizeof(MtcToken) + strlen(str));
	
	res->type = type; 
	res->num = 0;
	strcpy(res->str, str);
	res->next = res->prev = NULL;
	res->location = NULL;
	res->nl_hint = 0;
	
	return res;
}

void mtc_token_copy_location(MtcToken *to, MtcToken *from)
{
	if (to->location)
		mtc_source_ptr_free(to->location);
	
	to->location = mtc_source_ptr_copy(from->location);
	to->nl_hint = from->nl_hint;
}

//Adds a reference location to all tokens in list
void mtc_token_list_add_ref
	(MtcToken *head, const MtcSourcePtr *location)
{
	MtcToken *iter;
	
	for (iter = head; iter; iter = iter->next)
	{
		if (iter->location)
			mtc_source_ptr_append_copy
				(iter->location, location, MTC_SOURCE_INV_REF);
	}
}

//Debugging functions
void mtc_token_list_dump(MtcToken *token, FILE *stream)
{
	MtcToken *iter;
	
	fprintf(stream, "Regenerated source: \n");
	for (iter = token; iter; iter = iter->next)
	{
		if (iter->nl_hint)
			putc('\n', stream);
		if (iter->type == MTC_TOKEN_ID)
			fprintf(stream, "%s ", iter->str);
		if (iter->type == MTC_TOKEN_NUM)
			fprintf(stream, "%d ", (int) iter->num);
		if (iter->type == MTC_TOKEN_STR)
			fprintf(stream, "\"%s\" ", iter->str);
		if (iter->type == MTC_TOKEN_SYM)
			fprintf(stream, "%s ", mtc_sc_to_str[iter->num]);
		
	}
	
	fprintf(stream, "\n\nTokens: \n");
	for (iter = token; iter; iter = iter->next)
	{
		if (iter->type == MTC_TOKEN_ID)
			fprintf(stream, "Identifier %s ", iter->str);
		if (iter->type == MTC_TOKEN_NUM)
			fprintf(stream, "Number %d ", (int) iter->num);
		if (iter->type == MTC_TOKEN_STR)
			fprintf(stream, "String \"%s\" ", iter->str);
		if (iter->type == MTC_TOKEN_SYM)
			fprintf(stream, "Symbol \'%s\' ", mtc_sc_to_str[iter->num]);
		fprintf(stream, "(nl_hint = %d) ", iter->nl_hint);
		mtc_source_ptr_write(iter->location, stream);
	}
}

//MtcTokenIter

//Initializes the iterator
void mtc_token_iter_init(MtcTokenIter *self, MtcToken *start, int len)
{
	self->start = start;
	self->cur = start;
	if (start)
		self->next = start->next;
	else
		self->next = NULL;
	self->prev = NULL;
	self->pos = 0;
	self->len = len;
}

//Moves iterator ahead. Returns current token after moving.
MtcToken *mtc_token_iter_next(MtcTokenIter *self)
{
	if (self->pos < self->len)
	{
		self->pos++;
		self->prev = self->cur;
		self->cur = self->next;
		if (self->next)
			self->next = self->next->next;
	}
	return self->cur;
}

//Moves iterator backward. Returns current token after moving.
MtcToken *mtc_token_iter_prev(MtcTokenIter *self)
{
	if (self->pos > 0)
	{
		self->pos--;
		self->next = self->cur;
		self->cur = self->prev;
		if (self->prev)
			self->prev = self->prev->prev;
	}
	return self->cur;
}

//Moves iterator back to starting point
MtcToken *mtc_token_iter_home(MtcTokenIter *self)
{
	self->cur = self->start;
	if (self->start)
		self->next = self->start->next;
	else
		self->next = NULL;
	self->prev = NULL;
	self->pos = 0;
	
	return self->cur;
}

//Removes n_tokens tokens leftwards starting from iterator's
//current position. Returns no. of tokens actually removed. 
int mtc_token_iter_del
	(MtcTokenIter *self, int n_tokens, MtcToken **removed)
{
	MtcToken *rm_head, *rm_tail, *new_cur;
	int i, n_del;
	
	//Find first and last token to be removed
	n_del = self->pos + 1;
	if (n_del > n_tokens)
		n_del = n_tokens;
		
	//Alternate code if
	if (n_del == 0)
	{
		if (removed)
			*removed = NULL;
		return 0;
	}
	
	for (i = n_del - 1, rm_head = self->cur; 
		i; i--, rm_head = rm_head->prev)
		;
	rm_tail = self->cur;
	
	//Find new value of self->cur
	if (rm_head)
		new_cur = rm_head->prev;
	else
		new_cur = NULL;
	
	//Do the removal process
	if (rm_head)
	{
		rm_head->prev = NULL;
		rm_tail->next = NULL;
	}
	if (new_cur)
		new_cur->next = self->next;
	if (self->next)
		self->next->prev = new_cur;
	
	//Update pointers
	self->cur = new_cur;
	if (self->cur)
		self->prev = self->cur->prev;
	if (self->start == rm_head)
		self->start = self->next;
	
	//Update indices
	self->pos -= n_del;
	self->len -= n_del;
	
	//Return/destroy removed token
	if (removed)
		*removed = rm_head;
	else
		mtc_token_list_free(rm_head);
	
	return n_del;
}

void mtc_token_iter_ins_left(MtcTokenIter *self, MtcToken *head)
{
	MtcToken *tail;
	int n;
	
	//Assertion
	if (! head || ! self->cur)
		mtc_error("Assertion failed");
	
	//Find out tail and count the tokens
	for (tail = head, n = 1; tail->next; tail = tail->next, n++)
		;
	
	//Do the insertion
	if (self->cur)
		self->cur->next = head;
	if (self->next)
		self->next->prev = tail;
	head->prev = self->cur;
	tail->next = self->next;
	
	//Update
	self->cur = tail;
	self->prev = tail->prev;
	self->pos += n;
	self->len += n;
}

void mtc_token_iter_ins_right(MtcTokenIter *self, MtcToken *head)
{
	MtcToken *tail;
	int n;
	
	//Assertion
	if (! head || ! self->cur)
		mtc_error("Assertion failed");
	
	//Find out tail and count the tokens
	for (tail = head, n = 1; tail->next; tail = tail->next, n++)
		;
	
	//Do the insertion
	if (self->cur)
		self->cur->next = head;
	if (self->next)
		self->next->prev = tail;
	head->prev = self->cur;
	tail->next = self->next;
	
	//Update
	self->next = tail;
	self->len += n;
}


//Lexical scanner

typedef struct
{
	MtcSource *source;
	MtcSourceMsgList *el;
	int pos;
	int rhpos;
	int max;
	int lim;
	int lineno;
	int line_offset;
	int cur;
	int rh;
} MtcScanner;

//Initialize the state
static void mtc_scanner_init
	(MtcScanner *self, MtcSource *source, MtcSourceMsgList *el)
{
	self->source = source;
	self->el = el;
	self->pos = -1;
	self->rhpos = -1;
	self->lim = source->n_chars;
	self->max = source->n_chars - 1;
	self->lineno = 0;
	self->line_offset = -1;
	self->cur = EOF;
	self->rh = EOF;
}

//Create a location from given state
static MtcSourcePtr *mtc_scanner_locate
	(MtcScanner *self, MtcScanner *start)
{
	int line;
	MtcSourcePtr *res = NULL;
	
	for (line = self->lineno; line >= start->lineno; line--)
	{
		int offset, len;
		MtcSourceInvType inv_type = MTC_SOURCE_INV_CAT;
		
		//Calculate range
		offset = 0;
		len = MTC_SOURCE_LINE_LEN(self->source, line);
		if (line == self->lineno)
		{
			len = self->line_offset;
		}
		if (line == start->lineno)
		{
			offset = start->line_offset;
			len -= start->line_offset;
			inv_type = MTC_SOURCE_INV_DIRECT;
		}
		
		//Create new MtcSourcePtr structure
		res = mtc_source_ptr_new
			(res, inv_type, self->source, line, offset, len);
	}
	
	return res;
}

static MtcToken *mtc_token_from_vector
	(MtcToken *prev, MtcVector *buf, MtcTokenType type, 
		MtcScanner *cur, MtcScanner *start, int nl_hint)
{
	int buf_len;
	MtcToken *token;
	
	buf_len = mtc_vector_n_elements(buf, char);
	token = (MtcToken *) mtc_alloc(sizeof(MtcToken) + buf_len);
	token->type = type;
	token->location = mtc_scanner_locate(cur, start);
	token->num = 0;
	token->prev = prev;
	memcpy(token->str, mtc_vector_first(buf, char), buf_len);
	token->str[buf_len] = '\0';
	token->nl_hint = nl_hint;
	
	return token;
}

static MtcToken *mtc_token_from_num
	(MtcToken *prev, uint64_t num, MtcTokenType type,
	 MtcScanner *cur, MtcScanner *start, int nl_hint)
{
	MtcToken *token;
	
	token = (MtcToken *) mtc_alloc(sizeof(MtcToken));
	token->type = type;
	token->location = mtc_scanner_locate(cur, start);
	token->num = num;
	token->prev = prev;
	token->str[0] = 0;
	token->nl_hint = nl_hint;
	
	return token;
}

//Updates information about current position
static int mtc_scanner_update(MtcScanner *self)
{	
	//Move to correct line first
	for (; self->lineno < self->source->n_lines; self->lineno++)
	{
		if (self->source->lines[self->lineno] >= self->pos)
			break;
	}
	
	//Now set line_offset
	self->line_offset = self->pos
		- MTC_SOURCE_LINE_START(self->source, self->lineno);
	
	return 0;
}

//Advances forward, taking care of backslashes
static int mtc_scanner_iter(MtcScanner *self)
{
	//Dont move past end of source
	if (self->rhpos >= self->lim)
		return -1;
	
	//Copy from read-ahead to current
	self->pos = self->rhpos;
	self->cur = self->rh;
	
	//Dont step on backslash escaped \n's.
	while ((self->max - self->rhpos >= 2)
	&& (MTC_SOURCE_NTH_CHAR(self->source, self->rhpos + 1) == '\\')
	&& (MTC_SOURCE_NTH_CHAR(self->source, self->rhpos + 2) == '\n'))
	{
		self->rhpos += 2;
	}
	
	//Move ahead
	self->rhpos++;
	
	//Update
	if (self->rhpos < self->lim)
		self->rh = MTC_SOURCE_NTH_CHAR(self->source, self->rhpos);
	else
		self->rh = EOF;
	
	return 0;
}

//Reads an integer value, base has to be predetermined
static int64_t mtc_scanner_read_integer
	(MtcScanner *self, int base, int noerror)
{
	int64_t val = 0;
	
	do
	{
		int digit;
		
		//Read a digit
		if (self->cur  >= '0' && self->cur <= '9')
			digit = self->cur - '0';
		else if (self->cur >= 'a' && self->cur <= 'f')
			digit = self->cur - 'a' + 10;
		else if (self->cur >= 'A' && self->cur <= 'F')
			digit = self->cur - 'A' + 10;
		else if (self->cur == '_')
			continue;
		else
			digit = 16; //anything >= 16 will do
		
		if (digit >= base)
		{
			MtcSourcePtr *location;
			
			if (noerror)
				break;
			
			//Report error
			location = mtc_source_ptr_new
				(NULL, MTC_SOURCE_INV_DIRECT, self->source,
				self->lineno, self->line_offset, 1);
			mtc_source_msg_list_add
				(self->el, location, MTC_SOURCE_MSG_ERROR,
				"Unrecognized character \'%c\' in "
			    "numeric constant of base %d", 
			    self->cur, base);
			mtc_source_ptr_free(location);
			continue;
		}
		
		val *= base;
		val += digit;
	} while ((mtc_scanner_iter(self) == 0) 
				&& mtc_isid(self->cur));
	
	return val;
}

//Reads the source and tokenizes it
int mtc_scanner_scan
	(MtcSource *source, MtcToken **res, MtcSourceMsgList *el)
{
	MtcScanner self, ss1;
	MtcToken *stack = NULL;
	MtcVector buf;
	int token_count = 0;
	int stat;
	int nl_hint = 1;
	
	mtc_scanner_init(&self, source, el);
	mtc_scanner_iter(&self);
	stat = mtc_scanner_iter(&self);
	mtc_vector_init(&buf);
	
	while (stat == 0)
	{	
		//Handle whitespaces
		if (isspace(self.cur))
		{
			if (self.cur == '\n')
				nl_hint = 1;
			
			stat = mtc_scanner_iter(&self);
			continue;
		}
		
		//Filter '//' comments
		if (self.cur == '/' && self.rh == '/')
		{
			while (mtc_scanner_iter(&self) == 0)
			{
				if (self.cur == '\n')
					break;
			}
			
			stat = mtc_scanner_iter(&self);
			continue;
		}
		
		//Filter '/*...*/' comments
		if (self.cur == '/' && self.rh == '*')
		{
			int comment_open = 1;
			MtcSourcePtr *location;
			
			while (mtc_scanner_iter(&self) == 0)
			{
				if (self.cur == '*' && self.rh == '/')
				{
					comment_open = 0;
					break;
				}
			}
			
			//If the comment wasnt closed add an error
			if (comment_open)
			{
				location = mtc_source_ptr_new
					(NULL, MTC_SOURCE_INV_DIRECT, self.source,
					ss1.lineno, ss1.line_offset, 2);
				mtc_source_msg_list_add
					(self.el, location, MTC_SOURCE_MSG_ERROR,
					"Unterminated multiline comment");
				mtc_source_ptr_free(location);
				break;
			}
			else
			{
				mtc_scanner_iter(&self);
				stat = mtc_scanner_iter(&self);
				continue;
			}
		}
		
		//Initialize for a new token
		mtc_scanner_update(&self);
		ss1 = self;
		mtc_vector_resize(&buf, 0);
		
		//Identifiers
		if (mtc_isid_1(self.cur))
		{
			//Read the entire identifier
			do
			{
				mtc_vector_grow(&buf, 1);
				*mtc_vector_last(&buf, char) = self.cur;
			} while ((mtc_scanner_iter(&self) == 0) 
				&& mtc_isid(self.cur));
			
			//Create a token for it
			mtc_scanner_update(&self);
			stack = mtc_token_from_vector
				(stack, &buf, MTC_TOKEN_ID, &self, &ss1, nl_hint);
			nl_hint = 0;
			
			continue;
		}
		
		//Numeric values
		if (isdigit(self.cur))
		{
			int base = 10;
			uint64_t num;
			
			if (self.cur == '0')
			{
				if (self.rh == 'X' || self.rh == 'x')
				{
					//Hexadecimal
					base = 16;
					mtc_scanner_iter(&self);
					mtc_scanner_iter(&self);
				}
				else
				{
					//Octal
					base = 8;
				}
			}
			
			num = mtc_scanner_read_integer(&self, base, 0);
			
			//Create a token for it
			mtc_scanner_update(&self);
			stack = mtc_token_from_num
				(stack, num, MTC_TOKEN_NUM, &self, &ss1, nl_hint);
			nl_hint = 0;
			
			continue;
		}
		
		//String
		if (self.cur == '\"')
		{
			int string_open = 1;
			
			//Read the entire string
			while ((stat = mtc_scanner_iter(&self)) == 0)
			{
				//String termination
				if (self.cur == '\"')
				{
					string_open = 0;
					break;
				}
				
				//String must end in same line
				if (self.cur == '\n')
					break;
				
				//escape sequences
				if (self.cur == '\\')
				{
					MtcSourcePtr *location;
					
					if ((stat = mtc_scanner_iter(&self)) != 0)
						break;
					
					if (self.cur == '\\' || self.cur == '?' || self.cur == '\''
						|| self.cur == '\"')
						;
					else if (self.cur == 'a')      
						self.cur = '\a';
					else if (self.cur == 'b') 
						self.cur = '\b';
					else if (self.cur == 'f')
						self.cur = '\f';
					else if (self.cur == 'n')
						self.cur = '\n';
					else if (self.cur == 't')
						self.cur = '\t';
					else if (self.cur == 'v')
						self.cur = '\v';
					else if (self.cur == 'x' || self.cur == 'X' 
						|| isdigit(self.cur))
					{
						uint64_t num;
						
						int base = 8;
						
						if (self.cur == 'x' || self.cur == 'X')
						{
							base = 16;
							if ((stat = mtc_scanner_iter(&self)) != 0)
								break;
						}
						
						num = mtc_scanner_read_integer(&self, base, 1);
						
						if (num > 255)
						{
							location = mtc_source_ptr_new
								(NULL, MTC_SOURCE_INV_DIRECT, source,
								self.lineno, self.line_offset, 1);
							mtc_source_msg_list_add
							(self.el, location, MTC_SOURCE_MSG_ERROR,
							"Out of range value of escape sequence");
							mtc_source_ptr_free(location);
						}
						
						self.cur = num;
					}
					else
					{
						//Put up a warning
						location = mtc_source_ptr_new
							(NULL, MTC_SOURCE_INV_DIRECT, source,
							self.lineno, self.line_offset, 1);
						mtc_source_msg_list_add
							(self.el, location, MTC_SOURCE_MSG_WARN,
							"Unknown escape sequence");
						mtc_source_ptr_free(location);
					}
				}
				
				mtc_vector_grow(&buf, 1);
				*mtc_vector_last(&buf, char) = self.cur;
			}
			
			//If string is not closed throw an error
			if (string_open)
			{
				MtcSourcePtr *location;
				
				location = mtc_source_ptr_new
					(NULL, MTC_SOURCE_INV_DIRECT, self.source,
					ss1.lineno, ss1.line_offset, 1);
				mtc_source_msg_list_add
					(self.el, location, MTC_SOURCE_MSG_ERROR,
					"Unterminated string constant");
				mtc_source_ptr_free(location);
				break;
			}
			
			//Create a token for it
			mtc_scanner_iter(&self);
			mtc_scanner_update(&self);
			stack = mtc_token_from_vector
				(stack, &buf, MTC_TOKEN_STR, &self, &ss1, nl_hint);
			nl_hint = 0;
			
			continue;
		}
		
		//If reached here then it must be a special symbol.
		//Just add it
		
		
		//Special character(s)
		{
			int sch;
			
			//See what it is
			for (sch = 0; sch < MTC_SC_N; sch++)
			{
				if (self.cur == mtc_sc_to_str[sch][0])
				{
					if (! mtc_sc_to_str[sch][1])
						break;
					if (self.rh == mtc_sc_to_str[sch][1])
					{
						mtc_scanner_iter(&self);
						break;
					}
				}
			}
			
			//Move ahead
			stat = mtc_scanner_iter(&self);
			
			//If recognized, add it
			if (sch < MTC_SC_N)
			{
				mtc_scanner_update(&self);
				stack = mtc_token_from_num
					(stack, sch, MTC_TOKEN_SYM, &self, &ss1, nl_hint);
				nl_hint = 0;
			}
			//else throw an error
			else
			{
				MtcSourcePtr *location;
				
				location = mtc_source_ptr_new
					(NULL, MTC_SOURCE_INV_DIRECT, self.source,
					ss1.lineno, ss1.line_offset, 1);
				mtc_source_msg_list_add
					(self.el, location, MTC_SOURCE_MSG_ERROR,
					"Unrecognized character");
				mtc_source_ptr_free(location);
			}
		}
	}
	
	//Destroy the buffer
	mtc_vector_destroy(&buf);
	
	//Finish the doubly linked list
	if (stack)
	{
		MtcToken *next = NULL;
		while (1)
		{
			stack->next = next;
			next = stack;
			token_count++;
			
			if (! stack->prev)
				break;
			stack = stack->prev;
		}
	}
	
	//Return 
	*res = stack;
	return token_count;
}

