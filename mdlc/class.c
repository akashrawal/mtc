/* class.c
 * Dealing with classes
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

//'One way serializer'

void mtc_write_one_way_serializers
		(MtcSymbolClass *klass, MtcSymbolVar *list, MtcDLen base_size,
		char *member, char *as, char *sn, char *dsn, 
		char *member_ptr_type, int idx,
		FILE *h_file, FILE *c_file)
//as: argument structure
//sn: serializer name
//dsn: deserializer name
{
	if (list)
	{
		//With arguments (structure, serializer deserializer)
		
		MtcSymbolVar *iter;
		
		//Write the structure definition
		fprintf(h_file, 
			"typedef struct\n"
			"{\n");
		
		for (iter = list; iter; 
			iter = (MtcSymbolVar *) iter->parent.next)
		{
			fprintf(h_file, "    ");
			mtc_var_gen(iter, h_file);
			fprintf(h_file, ";\n");
		}
		
		fprintf(h_file, "} %s__%s__%s;\n\n", 
			klass->parent.name, member, as);
		
		//Serialiation function:
		
		//The beginning part
		fprintf(h_file, 
			"MtcMsg *%s__%s__%s(%s__%s__%s *args);\n\n",
			klass->parent.name, member, sn,
			klass->parent.name, member, as);
		
		fprintf(c_file, 
			"MtcMsg *%s__%s__%s(%s__%s__%s *args)\n{\n",
			klass->parent.name, member, sn,
			klass->parent.name, member, as);
		
		//Declarations
		fprintf(c_file, 
			"    uint32_t member_ptr = %s | %d;\n"
			"    MtcMsg *res;\n"
			"    MtcDLen size = {%d, %d};\n"
			"    MtcSegment seg_v;\n"
			"    MtcSegment *seg = &seg_v;\n"
			"    MtcDStream dstream_v;\n"
			"    MtcDStream *dstream = &dstream_v;\n\n", 
			member_ptr_type, idx, 
			(int) base_size.n_bytes + 4, 
			(int) base_size.n_blocks);
		
		//Count size of the message
		mtc_var_list_code_for_count(list, "args->", c_file);
		
		//Create a message
		fprintf(c_file, 
			"    res = mtc_msg_new"
			"(size.n_bytes, size.n_blocks);\n"
			"    mtc_msg_iter(res, dstream);\n"
			"    mtc_dstream_get_segment(dstream, %d, %d, seg);\n\n",
			(int) base_size.n_bytes + 4, 
			(int) base_size.n_blocks);
		
		//Write function number as member pointer
		fprintf(c_file, 
			"    mtc_segment_write_uint32(seg, member_ptr);\n\n");
		
		//Write functon arguments
		mtc_var_list_code_for_write(list, "args->", c_file);
		
		//Write function return value
		fprintf(c_file, "\n"
			"    return res;\n"
			"}\n\n");
		
		//Free function: 
		fprintf(h_file, 
			"void %s__%s__%s_free(%s__%s__%s *args);\n\n",
			klass->parent.name, member, as,
			klass->parent.name, member, as);
		
		fprintf(c_file, 
			"void %s__%s__%s_free(%s__%s__%s *args)\n{\n",
			klass->parent.name, member, as,
			klass->parent.name, member, as);
		mtc_var_list_code_for_free(list, "args->", c_file);
		fprintf(c_file, "}\n\n");
		
		//Deserialization function:
		
		//The beginning part
		fprintf(h_file, 
			"int %s__%s__%s(MtcMsg *msg, %s__%s__%s *args);\n\n",
			klass->parent.name, member, dsn,
			klass->parent.name, member, as);
		fprintf(c_file, 
			"int %s__%s__%s(MtcMsg *msg, %s__%s__%s *args)\n{\n",
			klass->parent.name, member, dsn,
			klass->parent.name, member, as);
		
		//Declarations
		fprintf(c_file, 
			"    MtcSegment seg_v;\n"
			"    MtcSegment *seg = &seg_v;\n"
			"    MtcDStream dstream_v;\n"
			"    MtcDStream *dstream = &dstream_v;\n\n");
		
		//Get the segment
		fprintf(c_file, 
			"    mtc_msg_iter(msg, dstream);\n"
			"    dstream->bytes += 4;\n"
			"    if (mtc_dstream_get_segment(dstream, %d, %d, seg) < 0)\n"
			"        goto _mtc_return;\n\n",
			(int) base_size.n_bytes, 
			(int) base_size.n_blocks);
		
		//Read the data
		mtc_var_list_code_for_read(list, "args->", c_file);
		
		//Message should be empty
		fprintf(c_file, 
			"    if (! mtc_dstream_is_empty(dstream))\n"
			"        goto _mtc_destroy_n_return;\n"
			"    mtc_msg_unref(msg);\n"
			"    return 0;\n\n");
		
		//Failure handling code
		fprintf(c_file, "_mtc_destroy_n_return:\n");
		mtc_var_list_code_for_read_fail(list, "args->", c_file);
		fprintf(c_file, "_mtc_return:\n"
			"    mtc_msg_unref(msg);\n"
			"    return -1;\n"
			"}\n\n");
	}
	else
	{
		//Without arguments (only serializer)
		
		//Serialiation function:
		
		fprintf(h_file, 
			"MtcMsg *%s__%s__%s(/*unused*/ void *args);\n\n",
			klass->parent.name, member, sn);
		
		fprintf(c_file, 
			"MtcMsg *%s__%s__%s(void *args)\n"
			"{\n"
			"    MtcMsg *msg;\n"
			"    MtcMBlock byte_stream;\n"
			"    uint32_t member_ptr = %s | %d;\n"
			"\n"
			"    msg = mtc_msg_new(4, 0);"
			"\n"
			"    byte_stream = mtc_msg_get_blocks(msg)[0];\n"
			"\n"
			"    mtc_uint32_copy_to_le(byte_stream.data, &member_ptr);\n"
			"\n"
			"    return msg;\n"
			"}\n\n",
			klass->parent.name, member, sn,
			member_ptr_type, idx);
	}
}

//Now generate code for the class

void mtc_class_gen_code
		(MtcSymbolClass *klass, FILE *h_file, FILE *c_file)
{
	MtcSymbolFunc *fn;
	MtcSymbolClass *parent;
	int n_fns = 0, n_parent_fns = 0, i;
	
	//Counting
	for (parent = klass->parent_class; parent; 
		parent = parent->parent_class)
	{
		for (fn = parent->funcs; fn; 
			fn = (MtcSymbolFunc *) fn->parent.next)
		{
			n_parent_fns++;
		}
	}
	
	for (fn = klass->funcs; fn; 
		fn = (MtcSymbolFunc *) fn->parent.next)
	{
		n_fns++;
	}
	
	//Mark the beginnings
	fprintf(h_file, "//class %s\n\n", klass->parent.name);
	fprintf(c_file, "//class %s\n\n", klass->parent.name);
	
	//Write functions
	for (fn = klass->funcs, i = n_parent_fns; fn; 
		fn = (MtcSymbolFunc *) fn->parent.next, i++)
	{
		//Comment
		fprintf(h_file, "//%s::%s\n\n", 
			klass->parent.name, fn->parent.name);
		fprintf(c_file, "//%s::%s\n\n", 
			klass->parent.name, fn->parent.name);
		
		//Write its index
		fprintf(h_file, "#define %s__%s__IDX %d\n\n",
			klass->parent.name, fn->parent.name, i);
		
		
		//Write code for input arguments
		mtc_write_one_way_serializers
			(klass, fn->in_args, fn->in_args_base_size, 
			fn->parent.name, "in_args", "msg", "read",
			"MTC_MEMBER_PTR_FN", i, 
			h_file, c_file);
		
		//Write code for output arguments,
		mtc_write_one_way_serializers
			(klass, fn->out_args, fn->out_args_base_size, 
			fn->parent.name, "out_args", "reply", "finish",
			"MTC_MEMBER_PTR_FN", 0, 
			h_file, c_file);
	}
	
	
	//Print binaries
	fprintf(h_file, "//%s: Binaries\n\n", 
		klass->parent.name);
	fprintf(c_file, "//%s: Binaries\n\n", 
		klass->parent.name);
	
	fprintf(h_file, 
		"#define mtc_fc_list__%s \\\n", klass->parent.name);
	
	if (klass->parent_class)
	{
		fprintf(h_file,
			"    mtc_fc_list__%s,\\\n", 
			klass->parent_class->parent.name);
	}
	
	for (fn = klass->funcs, i = n_parent_fns; fn; 
		fn = (MtcSymbolFunc *) fn->parent.next, i++)
	{
		fprintf(h_file, 
			"    {\\\n"
			"        %s__%s__IDX,\\\n", 
			klass->parent.name, fn->parent.name);
		
		if (fn->in_args)
		{
			fprintf(h_file, 
				"        sizeof(%s__%s__in_args),\\\n"
				"        (MtcSerFn) %s__%s__msg,\\\n"
				"        (MtcDeserFn) %s__%s__read,\\\n"
				"        (MtcFreeFn) %s__%s__in_args_free,\\\n",
				klass->parent.name, fn->parent.name,
				klass->parent.name, fn->parent.name,
				klass->parent.name, fn->parent.name,
				klass->parent.name, fn->parent.name);
		}
		else
		{
			fprintf(h_file, 
				"        0,\\\n"
				"        (MtcSerFn) %s__%s__msg,\\\n"
				"        (MtcDeserFn) NULL,\\\n"
				"        (MtcFreeFn) NULL,\\\n",
				klass->parent.name, fn->parent.name);
		}
		
		if (fn->out_args)
		{
			fprintf(h_file, 
				"        sizeof(%s__%s__out_args),\\\n"
				"        (MtcSerFn) %s__%s__reply,\\\n"
				"        (MtcDeserFn) %s__%s__finish,\\\n"
				"        (MtcFreeFn) %s__%s__out_args_free\\\n",
				klass->parent.name, fn->parent.name,
				klass->parent.name, fn->parent.name,
				klass->parent.name, fn->parent.name,
				klass->parent.name, fn->parent.name);
		}
		else
		{
			fprintf(h_file, 
				"        0,\\\n"
				"        (MtcSerFn) %s__%s__reply,\\\n"
				"        (MtcDeserFn) NULL,\\\n"
				"        (MtcFreeFn) NULL\\\n",
				klass->parent.name, fn->parent.name);
		}
		
		if (fn->parent.next)
			fprintf(h_file, "    },\\\n");
		else
			fprintf(h_file, "    }\n\n");
	}
	
	fprintf(h_file, 
		"extern MtcFCBinary mtc__%s__fns[%d];\n\n",
		klass->parent.name, n_parent_fns + n_fns);
	
	fprintf(c_file, 
		"MtcFCBinary mtc__%s__fns[%d] = { mtc_fc_list__%s };\n\n",
		klass->parent.name, n_parent_fns + n_fns, klass->parent.name);
	
	fprintf(h_file, 
		"extern MtcClassBinary %s;\n\n", klass->parent.name);
	fprintf(c_file,
		"MtcClassBinary %s = {%d, mtc__%s__fns};\n\n",
		klass->parent.name,
		n_parent_fns + n_fns, 
		klass->parent.name);
	
	for (fn = klass->funcs, i = n_parent_fns; fn; 
		fn = (MtcSymbolFunc *) fn->parent.next, i++)
	{
		fprintf(h_file, 
		"#define %s__%s (mtc__%s__fns + %d);\n\n",
		klass->parent.name, fn->parent.name, 
		klass->parent.name, i);
	}
}
