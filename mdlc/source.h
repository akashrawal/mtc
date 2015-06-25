/* source.h
 * MtcSource that represents an MDL source file in memory
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


//Source file (stores contents in memory)
typedef struct _MtcSource MtcSource;
#define MTC_SOURCE_LINE_START(source, n) \
	((source)->lines[(n)])
#define MTC_SOURCE_LINE_LEN(source, n) \
	((source)->lines[(n) + 1] - (source)->lines[(n)])
struct _MtcSource
{
	//Name of the source
	char *name;
	//Contents of the source
	char *chars;
	size_t n_chars;
	//Offset of each line beginning
	int *lines;
	size_t n_lines;
	
	int refcount;
};

void mtc_source_ref(MtcSource *self);
void mtc_source_unref(MtcSource *self);
MtcSource *mtc_source_new_from_stream(const char *name, int fd);
MtcSource *mtc_source_new_from_file(const char *filename);
int mtc_source_get_lineno(MtcSource *self, int pos);

//Describes in what way a particular source is involved in describing
//any symbol/token
typedef enum 
{
	//Direct involvement (the symbol/token is directly there)
	MTC_SOURCE_INV_DIRECT = 0,
	//A part of the symbol/token is here, former part is in previous element
	MTC_SOURCE_INV_CAT = 1,
	//Indirect involvement through macro expansion
	MTC_SOURCE_INV_MACRO = 2,
	//Indirect involvement through file inclusion
	MTC_SOURCE_INV_INCLUDE = 3,
	//Indirect involvement through file reference
	MTC_SOURCE_INV_REF = 4
} MtcSourceInvType;

//A linked list that describes where in the sources a partiular 
//symbol/token came from
typedef struct _MtcSourcePtr MtcSourcePtr;
struct _MtcSourcePtr
{
	MtcSourcePtr *next;
	MtcSource *source;
	MtcSourceInvType inv_type;
	int start, len;
	//start is from beginning of the source
};

MtcSourcePtr *mtc_source_ptr_new
	(MtcSourcePtr *next, MtcSourceInvType inv_type, MtcSource *source,
	int start, int len);
MtcSourcePtr *mtc_source_ptr_copy(const MtcSourcePtr *sptr);
void mtc_source_ptr_free(MtcSourcePtr *sptr);
void mtc_source_ptr_append
	(MtcSourcePtr *sptr, MtcSourcePtr *after, 
	MtcSourceInvType iface_inv);
void mtc_source_ptr_append_copy
	(MtcSourcePtr *sptr, const MtcSourcePtr *after, 
	MtcSourceInvType iface_inv);
void mtc_source_ptr_write(MtcSourcePtr *sptr, FILE *stream);

//Types of messages that MDL compiler can throw
typedef enum 
{
	MTC_SOURCE_MSG_ERROR,
	MTC_SOURCE_MSG_WARN
} MtcSourceMsgType;

//An message pointing to a source file
typedef struct _MtcSourceMsg MtcSourceMsg;
struct _MtcSourceMsg
{
	MtcSourceMsg *next;
	MtcSourcePtr *location;
	MtcSourceMsgType type;
	char *message;
};

//An accumulator for messages
typedef struct 
{
	MtcSourceMsg *head, *tail;
	int error_count, warn_count;
} MtcSourceMsgList;

MtcSourceMsgList *mtc_source_msg_list_new();
void mtc_source_msg_list_add
	(MtcSourceMsgList *self, MtcSourcePtr *location, 
	MtcSourceMsgType type, const char *format, ...);
void mtc_source_msg_list_dump(MtcSourceMsgList *self, FILE *stream);
void mtc_source_msg_list_free(MtcSourceMsgList *self);

