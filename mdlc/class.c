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
	//TODO: Fix for serializations wit no arguments
	//if (list)
	//{
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
		
		//TODO: delete
		/*
		//Expect message in reply to a function call?
		if (msg_in_reply)
			fprintf(c_file, 
				"\n    res->da_flags |= MTC_DA_FLAG_MSG_IN_REPLY;\n");
				*/
		
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
	//}
	//TODO: Delete
	/*
	else if (fn_idx >= 0)
	{
		//Serialiation function:
		
		//The beginning part
		fprintf(h_file, 
			"MtcMsg *%s__%s__%s();\n\n",
			klass->parent.name, member, sn);
		
		fprintf(c_file, 
			"MtcMsg *%s__%s__%s()\n{\n",
			klass->parent.name, member, sn);
		
		//Declarations
		fprintf(c_file, 
			"    uint32_t member_ptr = MTC_MEMBER_PTR_FN | %d;\n"
			"    MtcMsg *res;\n"
			"    MtcDLen size = {4, 0};\n"
			"    MtcSegment seg_v;\n"
			"    MtcSegment *seg = &seg_v;\n"
			"    MtcDStream dstream_v;\n"
			"    MtcDStream *dstream = &dstream_v;\n\n", 
			fn_idx);
		
		//Count size of the message
		mtc_var_list_code_for_count(list, "args->", c_file);
		
		//Create a message
		fprintf(c_file, 
			"    res = mtc_msg_new"
			"(size.n_bytes, size.n_blocks);\n"
			"    mtc_msg_iter(res, dstream);\n"
			"    mtc_dstream_get_segment(dstream, 4, 0, seg);\n\n");
		
		//Write function number as member pointer
		if (fn_idx >= 0)
			fprintf(c_file, 
				"    mtc_segment_write_uint32(seg, member_ptr);\n\n");
		
		//Expect message in reply to a function call?
		if (msg_in_reply)
			fprintf(c_file, 
				"\n    res->da_flags |= MTC_DA_FLAG_MSG_IN_REPLY;\n");
		
		//Write function return value
		fprintf(c_file, "\n"
			"    return res;\n"
			"}\n\n");
	}
	*/
}

//Now generate code for the class

void mtc_class_gen_code
		(MtcSymbolClass *klass, FILE *h_file, FILE *c_file)
{
	MtcSymbolFunc *fn;
	MtcSymbolEvent *evt;
	MtcSymbolClass *parent;
	int n_fns = 0, n_evts = 0, n_parent_fns = 0, n_parent_evts = 0, i;
	
	//Counting functions and events
	for (parent = klass->parent_class; parent; 
		parent = parent->parent_class)
	{
		for (fn = parent->funcs; fn; 
			fn = (MtcSymbolFunc *) fn->parent.next)
		{
			n_parent_fns++;
		}
		
		for (evt = parent->events; evt; 
			evt = (MtcSymbolEvent *) evt->parent.next)
		{
			n_parent_evts++;
		}
	}
	
	for (fn = klass->funcs; fn; 
		fn = (MtcSymbolFunc *) fn->parent.next)
	{
		n_fns++;
	}
	
	for (evt = klass->events; evt; 
		evt = (MtcSymbolEvent *) evt->parent.next)
	{
		n_evts++;
	}
	
	//Mark the beginnings
	fprintf(h_file, "//class %s\n\n", klass->parent.name);
	fprintf(c_file, "//class %s\n\n", klass->parent.name);
	
	//Print the class info
	fprintf(h_file, 
		"extern MtcClassInfo %s__info;\n\n", klass->parent.name);
	fprintf(c_file,
		"MtcClassInfo %s__info = {%d, %d};\n\n",
		klass->parent.name,
		n_parent_fns + n_fns, 
		n_parent_evts + n_evts);
	
	//For each function
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
		
		//TODO: Delete
		/*
		//Write code for input arguments
		mtc_write_one_way_serializers
			(klass, fn->in_args, fn->in_args_base_size, 
			fn->parent.name, "in_args", "msg", "read", i, 
			fn->out_args ? 1 : 0,
			h_file, c_file);
		
		//Write code for output arguments, if they are there
		if (fn->out_args)
		{
			mtc_write_one_way_serializers
				(klass, fn->out_args, fn->out_args_base_size, 
				fn->parent.name, "out_args", "reply", "finish", -1, 
				0,
				h_file, c_file);
		}
		*/
		
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
	
	//TODO: Event is for eventually
	/*
	for (evt = klass->events, i = n_parent_evts; evt; 
		evt = (MtcSymbolEvent *) evt->parent.next, i++)
	{
		//Comment
		fprintf(h_file, "//%s::%s\n\n", 
			klass->parent.name, evt->parent.name);
		fprintf(c_file, "//%s::%s\n\n", 
			klass->parent.name, evt->parent.name);
		
		//Write its index
		fprintf(h_file, "#define %s__%s__TAG %d\n\n",
			klass->parent.name, evt->parent.name, 
			MTC_EVENT_TAG_CREATE(i, (evt->args ? 1 : 0)));
		
		//Write code for event arguments if they are there
		if (evt->args)
		{
			mtc_write_one_way_serializers
				(klass, evt->args, evt->base_size, 
				evt->parent.name, "args", "msg", "read", -1, 
				0,
				h_file, c_file);
		}
	}
	*/
}
