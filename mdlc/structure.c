/* structure.c
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

#include "common.h"

#include <stdarg.h>

//These have to be kept in sync with enum MtcTypeFundamentalID

const char *mtc_c_names[] =
{
	"unsigned char",
	"uint16_t",
	"uint32_t", 
	"uint64_t",
	"char",
	"int16_t",
	"int32_t",
	"int64_t",
	"MtcValFlt",
	"MtcValFlt",
	"char*",
	"MtcMBlock",
	"MtcMsg*",
	NULL
};

//Tells whether the base type requires to be freed
int mtc_c_base_type_requires_free(MtcType type)
{
	if (type.cat == MTC_TYPE_FUNDAMENTAL)
	{
		if ((type.base.fid == MTC_TYPE_FUNDAMENTAL_RAW)
		    || (type.base.fid == MTC_TYPE_FUNDAMENTAL_STRING)
		    || (type.base.fid == MTC_TYPE_FUNDAMENTAL_MSG))
			return 1;
	}
	else
	{
		return 1;
	}
	
	return 0;
}

//Tells whether reference of the base type is baseless
int mtc_c_ref_type_is_baseless(MtcType type)
{
	if (type.cat == MTC_TYPE_FUNDAMENTAL)
	{
		if ((type.base.fid == MTC_TYPE_FUNDAMENTAL_RAW)
		    || (type.base.fid == MTC_TYPE_FUNDAMENTAL_STRING)
		    || (type.base.fid == MTC_TYPE_FUNDAMENTAL_MSG))
			return 1;
	}
	
	return 0;
}

//Tells whether reading a type can fail
int mtc_c_type_read_can_fail(MtcType type)
{
	if (type.complexity == MTC_TYPE_SEQ 
		|| type.complexity == MTC_TYPE_REF)
		return 1;
	if (type.cat == MTC_TYPE_USERDEFINED)
		return 1;
	if (type.base.fid == MTC_TYPE_FUNDAMENTAL_STRING)
		return 1;
	if (type.base.fid == MTC_TYPE_FUNDAMENTAL_MSG)
		return 1;
	
	return 0;
}

//Writes C base type for the type, ignoring complexity
void mtc_gen_base_type(MtcType type, FILE *output)
{	
	if (type.cat == MTC_TYPE_FUNDAMENTAL)
	{
		fprintf(output, "%s", mtc_c_names[type.base.fid]);
	}
	else
	{
		fprintf(output, "%s", type.base.symbol->name);
	}
}

//Writes C variable for given variable
void mtc_var_gen(MtcSymbolVar *var, FILE *output)
{
	//For normal type
	if (var->type.complexity == MTC_TYPE_NORMAL)
	{
		mtc_gen_base_type(var->type, output);
		fprintf(output, " %s", var->parent.name);
	}
	//For array
	else if (var->type.complexity > 0)
	{
		mtc_gen_base_type(var->type, output);
		fprintf(output, " %s[%d]", 
				var->parent.name, var->type.complexity);
	}
	//Sequence
	else if (var->type.complexity == MTC_TYPE_SEQ)
	{
		fprintf(output, "struct {");
		mtc_gen_base_type(var->type, output);
		fprintf(output, "* data; uint32_t len;} %s",
				var->parent.name);
	}
	//Reference
	else if (var->type.complexity == MTC_TYPE_REF)
	{
		mtc_gen_base_type(var->type, output);
		
		if (mtc_c_ref_type_is_baseless(var->type))
		{
			fprintf(output, " %s", var->parent.name);
		}
		else
		{
			fprintf(output, "* %s", var->parent.name);
		}
	}
}

//Writes an expression for the base lvalue of a variable
void mtc_var_code_base_exp
	(MtcSymbolVar *var, const char *prefix, FILE *c_file)
{
	//For normal type
	if (var->type.complexity == MTC_TYPE_NORMAL)
	{
		fprintf(c_file, "%s%s", prefix, var->parent.name);
	}
	//For array
	else if (var->type.complexity > 0)
	{
		fprintf(c_file, "%s%s[_i]", prefix, var->parent.name);
	}
	//Sequence
	else if (var->type.complexity == MTC_TYPE_SEQ)
	{
		fprintf(c_file, "%s%s.data[_i]", prefix, var->parent.name);
	}
	//Reference
	else if (var->type.complexity == MTC_TYPE_REF)
	{
		if (mtc_c_ref_type_is_baseless(var->type))
		{
			fprintf(c_file, "%s%s", prefix, var->parent.name);
		}
		else
		{
			fprintf(c_file, "*(%s%s)",prefix, var->parent.name);
		}
	}
}

//Writes test expression for reference variables
void mtc_var_code_ref_test_exp
	(MtcSymbolVar *var, const char *prefix, FILE *c_file)
{
	if ((var->type.cat == MTC_TYPE_FUNDAMENTAL)
		&& (var->type.base.fid == MTC_TYPE_FUNDAMENTAL_RAW))
	{
		fprintf(c_file, "%s%s.mem", prefix, var->parent.name);
	}
	else
	{
		fprintf(c_file, "%s%s",prefix, var->parent.name);
	}
}

//Writes code for serializing given base type. 
void mtc_var_code_for_base_write
	(MtcSymbolVar *var, const char *prefix, const char *segment, 
	 FILE *c_file)
{
	if (var->type.cat == MTC_TYPE_FUNDAMENTAL)
	{
		if (var->type.base.fid == MTC_TYPE_FUNDAMENTAL_MSG)
		{
			fprintf(c_file, "mtc_msg_write(");
			mtc_var_code_base_exp(var, prefix, c_file);
			fprintf(c_file, ", %s, dstream);\n", segment);
		}
		else
		{
			fprintf(c_file, "mtc_segment_write_%s(%s, ",
				mtc_type_fundamental_names[var->type.base.fid], 
				segment);
			mtc_var_code_base_exp(var, prefix, c_file);
			fprintf(c_file, ");\n");
		}
	}
	else
	{
		fprintf(c_file, "%s__write(&(", var->type.base.symbol->name);
		mtc_var_code_base_exp(var, prefix, c_file);
		fprintf(c_file, "), %s, dstream);\n", segment);
	}
}

//Writes code for freeing given base type. 
void mtc_var_code_for_base_free
	(MtcSymbolVar *var, const char *prefix, FILE *c_file)
{	
	if (var->type.cat == MTC_TYPE_FUNDAMENTAL)
	{
		if (var->type.base.fid == MTC_TYPE_FUNDAMENTAL_RAW)
		{
			fprintf(c_file, "mtc_rcmem_unref(");
			mtc_var_code_base_exp(var, prefix, c_file);
			fprintf(c_file, ".mem);\n");
		}
		else if (var->type.base.fid == MTC_TYPE_FUNDAMENTAL_STRING)
		{
			fprintf(c_file, "mtc_rcmem_unref(");
			mtc_var_code_base_exp(var, prefix, c_file);
			fprintf(c_file, ");\n");
		}
		else if (var->type.base.fid == MTC_TYPE_FUNDAMENTAL_MSG)
		{
			fprintf(c_file, "mtc_msg_unref(");
			mtc_var_code_base_exp(var, prefix, c_file);
			fprintf(c_file, ");\n");
		}
	}
	else
	{
		fprintf(c_file, "%s__free(&(", var->type.base.symbol->name);
		mtc_var_code_base_exp(var, prefix, c_file);
		fprintf(c_file, "));\n");
	}
}

//Writes code for deserializing given base type. 
//Returns 1 if the code is failable (we have to write {error handler} 
//after that in that case)
int mtc_var_code_for_base_read
	(MtcSymbolVar *var, const char *prefix, const char *segment, 
	 FILE *c_file)
{
	int failable = 0;
	
	if (var->type.cat == MTC_TYPE_FUNDAMENTAL)
	{	
		if (var->type.base.fid == MTC_TYPE_FUNDAMENTAL_STRING)
		{
			fprintf(c_file, "if (! (");
			mtc_var_code_base_exp(var, prefix, c_file);
			fprintf(c_file, " = mtc_segment_read_string(%s)))\n",
			        segment);
			failable = 1;
		}
		else if (var->type.base.fid == MTC_TYPE_FUNDAMENTAL_RAW
			|| var->type.base.fid == MTC_TYPE_FUNDAMENTAL_FLT32
			|| var->type.base.fid == MTC_TYPE_FUNDAMENTAL_FLT64)
		{
			fprintf(c_file, "mtc_segment_read_%s(%s, &(",
				mtc_type_fundamental_names[var->type.base.fid],
				segment);
			mtc_var_code_base_exp(var, prefix, c_file);
			fprintf(c_file, "));\n");
		}
		else if (var->type.base.fid == MTC_TYPE_FUNDAMENTAL_MSG)
		{
			fprintf(c_file, "if (! (");
			mtc_var_code_base_exp(var, prefix, c_file);
			fprintf(c_file, " = mtc_msg_read(%s, dstream)))\n",
			        segment);
			failable = 1;
		}
		else
		{
			fprintf(c_file, "mtc_segment_read_%s(%s, ",
				mtc_type_fundamental_names[var->type.base.fid],
				segment);
			mtc_var_code_base_exp(var, prefix, c_file);
			fprintf(c_file, ");\n");
		}
		
	}
	else
	{
		fprintf(c_file, "if (%s__read(&(", var->type.base.symbol->name);
		mtc_var_code_base_exp(var, prefix, c_file);
		fprintf(c_file, "), %s, dstream) < 0)\n", segment);
		failable = 1;
	}
	
	return failable;
}

//Writes code for assigning null values to references
void mtc_var_code_for_ref_null
	(MtcSymbolVar *var, const char *prefix, FILE *c_file)
{	
	if (var->type.cat == MTC_TYPE_FUNDAMENTAL)
	{
		if (var->type.base.fid == MTC_TYPE_FUNDAMENTAL_RAW)
			fprintf(c_file, "(%s%s).mem = NULL; (%s%s).size = 0;\n",
				prefix, var->parent.name, prefix, var->parent.name);
		else 
			fprintf(c_file, "%s%s = NULL;\n", prefix, var->parent.name);
	}
	else
		fprintf(c_file, "%s%s = NULL;\n", prefix, var->parent.name);
}

//Writes code to count dynamic size of a given list of variables
void mtc_var_list_code_for_count
	(MtcSymbolVar *list, const char *prefix, FILE *c_file)
{
	MtcSymbolVar *iter;
	
	for (iter = list; iter;
		iter = (MtcSymbolVar *) iter->parent.next)
	{
		const char *part1, *part2;
		const char *parts[3] = {"", "mtc_msg_count((", "__count(&("};
		
		//Prepare counting function
		if (iter->type.cat == MTC_TYPE_USERDEFINED)
		{
			part1 = iter->type.base.symbol->name;
			part2 = parts[2];
		}
		else
		{
			//Must be msg
			part1 = parts[0];
			part2 = parts[1];
		}
		
		fprintf(c_file,
			"    //%s%s\n",  prefix, iter->parent.name);
		if (iter->type.complexity == MTC_TYPE_NORMAL)
		{
			if (! mtc_base_type_is_constsize(iter->type))
			{
				//Only userdefined types
				fprintf(c_file, 
					"    {\n"
					"        MtcDLen onesize;\n"
					"        onesize = %s%s", 
					part1, part2);
				mtc_var_code_base_exp(iter, prefix, c_file);
				fprintf(c_file, 
					"));\n"
					"        size.n_bytes += onesize.n_bytes;\n"
					"        size.n_blocks += onesize.n_blocks;\n"
					"    }\n");
			}
		}
		else if (iter->type.complexity > 0)
		{
			if (! mtc_base_type_is_constsize(iter->type))
			{
				//Only userdefined types
				fprintf(c_file, 
					"    {\n"
					"        MtcDLen onesize;\n"
					"        int _i;\n"
					"        for (_i = 0; _i < %d; _i++)\n"
					"        {\n"
					"            onesize = %s%s",
					iter->type.complexity, 
					part1, part2);
				mtc_var_code_base_exp(iter, prefix, c_file);
				fprintf(c_file,
					"));\n"
					"            size.n_bytes += onesize.n_bytes;\n"
					"            size.n_blocks += onesize.n_blocks;\n"
					"        }\n"
					"    }\n");
			}
		}
		else if (iter->type.complexity == MTC_TYPE_SEQ)
		{
			//Find the base size
			MtcDLen base_size = mtc_base_type_calc_base_size(iter->type);
			
			//We need to add up base size
			fprintf(c_file, 
				"    size.n_bytes += %d * %s%s.len;\n"
				"    size.n_blocks += %d * %s%s.len;\n",
				(int) base_size.n_bytes, prefix, iter->parent.name,
				(int) base_size.n_blocks, prefix, iter->parent.name);
			
			if (! mtc_base_type_is_constsize(iter->type))
			{
				//Only userdefined types
				fprintf(c_file, 
					"    {\n"
					"        MtcDLen onesize;\n"
					"        int _i;\n"
					"        for (_i = 0; _i < %s%s.len; _i++)\n"
					"        {\n"
					"            onesize = %s%s",
					prefix, iter->parent.name, 
					part1, part2);
				mtc_var_code_base_exp(iter, prefix, c_file);
				fprintf(c_file,
					"));\n"
					"            size.n_bytes += onesize.n_bytes;\n"
					"            size.n_blocks += onesize.n_blocks;\n"
					"        }\n"
					"    }\n");
			}
		}
		else //reference
		{
			//Find the base size
			MtcDLen base_size = mtc_base_type_calc_base_size(iter->type);
			
			fprintf(c_file, 
				"    if (");
			mtc_var_code_ref_test_exp(iter, prefix, c_file);
			fprintf(c_file, 
				")\n"
				"    {\n"
				"        size.n_bytes += %d;\n"
				"        size.n_blocks += %d;\n",
				(int) base_size.n_bytes, (int) base_size.n_blocks);
				
			if (! mtc_base_type_is_constsize(iter->type))
			{
				
				//Only userdefined types
				
				fprintf(c_file, 
					"        {\n"
					"            MtcDLen onesize;\n"
					"            onesize = %s%s", 
					part1, part2);
				mtc_var_code_base_exp(iter, prefix, c_file);
				fprintf(c_file, 
					"));\n"
					"            size.n_bytes += onesize.n_bytes;\n"
					"            size.n_blocks += onesize.n_blocks;\n"
					"        }\n");
			}
			fprintf(c_file, 
				"    }\n");
		}
		fprintf(c_file, "\n");
	}
}

//Writes code to serialize a given list of variables
void mtc_var_list_code_for_write
	(MtcSymbolVar *list, const char *prefix, FILE *c_file)
{
	MtcSymbolVar *iter;
	
	for (iter = list; iter;
			iter = (MtcSymbolVar *) iter->parent.next)
	{
		fprintf(c_file,
			"    //%s%s\n",  prefix, iter->parent.name);
		
		//Simple types
		if (iter->type.complexity == MTC_TYPE_NORMAL)
		{
			fprintf(c_file, "    ");
			mtc_var_code_for_base_write(iter, prefix, "seg", c_file);
		}
		//Arrays
		else if (iter->type.complexity > 0)
		{
			fprintf(c_file, 
				"    {\n"
				"        int _i;\n"
				"        for (_i = 0; _i < %d; _i++)\n"
				"        {\n"
				"            ",
				iter->type.complexity);
			mtc_var_code_for_base_write(iter, prefix, "seg", c_file);
			fprintf(c_file,
				"        }\n"
				"    }\n");
				
		}
		//Sequences
		else if (iter->type.complexity == MTC_TYPE_SEQ)
		{
			//Find the base size
			MtcDLen base_size = mtc_base_type_calc_base_size(iter->type);
			
			fprintf(c_file, 
				"    {\n"
				"        int _i;\n"
				"        MtcSegment sub_seg;\n"
				"        \n"
				"        mtc_segment_write_uint32(seg, %s%s.len);\n"
				"        mtc_dstream_get_segment(dstream, "
				"%d * %s%s.len, %d * %s%s.len, &sub_seg);\n"
				"        for (_i = 0; _i < %s%s.len; _i++)\n"
				"        {\n"
				"            ",
				prefix, iter->parent.name, 
				(int) base_size.n_bytes, prefix, iter->parent.name,
				(int) base_size.n_blocks, prefix, iter->parent.name,
				prefix, iter->parent.name);
			mtc_var_code_for_base_write(iter, prefix, "&sub_seg", c_file);
			fprintf(c_file,
				"        }\n"
				"    }\n");
		}
		//Reference
		else if (iter->type.complexity == MTC_TYPE_REF)
		{
			//Find the base size
			MtcDLen base_size = mtc_base_type_calc_base_size(iter->type);
			
			fprintf(c_file, 
				"    if (");
			mtc_var_code_ref_test_exp(iter, prefix, c_file);
			fprintf(c_file, 
				")\n"
				"    {\n"
				"        MtcSegment sub_seg;\n"
				"        \n"
				"        mtc_segment_write_uchar(seg, 1);\n"
				"        mtc_dstream_get_segment(dstream, "
				"%d, %d, &sub_seg);\n"
				"        ", 
				(int) base_size.n_bytes, (int) base_size.n_blocks);
			mtc_var_code_for_base_write(iter, prefix, "&sub_seg", c_file);
			fprintf(c_file,
				"    }\n"
				"    else\n"
				"    {\n"
				"        mtc_segment_write_uchar(seg, 0);\n"
				"    }\n");
		}
		fprintf(c_file, "\n");
	}
}

//Writes code to deserialize a given list of variables: main code
void mtc_var_list_code_for_read
	(MtcSymbolVar *list, const char *prefix, FILE *c_file)
{
	MtcSymbolVar *iter;
	
	for (iter = list; iter;
			iter = (MtcSymbolVar *) iter->parent.next)
	{
		fprintf(c_file,
			"    //%s%s\n",  prefix, iter->parent.name);
		
		//Simple types
		if (iter->type.complexity == MTC_TYPE_NORMAL)
		{
			fprintf(c_file, 
					"    ");
			if (mtc_var_code_for_base_read(iter, prefix, "seg", c_file))
			{
				fprintf(c_file,
					"    {\n"
					"        goto _mtc_fail_%s;\n"
					"    }\n", iter->parent.name);
			}
		}
		//Arrays
		else if (iter->type.complexity > 0)
		{
			fprintf(c_file, 
				"    {\n"
				"        int _i;\n"
				"        for (_i = 0; _i < %d; _i++)\n"
				"        {\n"
				"            ",
				iter->type.complexity);
			if (mtc_var_code_for_base_read(iter, prefix, "seg", c_file))
			{
				fprintf(c_file, 
				"            {\n");
				if (mtc_c_base_type_requires_free(iter->type))
				{
					fprintf(c_file, 
				"                for (_i--; _i >= 0; _i--)\n"
				"                {\n"
				"                    ");
					mtc_var_code_for_base_free(iter, prefix, c_file);
					fprintf(c_file, 
				"                }\n");
				}
				fprintf(c_file,
				"                goto _mtc_fail_%s;\n"
				"            }\n", iter->parent.name);
			}
			fprintf(c_file,
				"        }\n"
				"    }\n");
				
		}
		//Sequences
		else if (iter->type.complexity == MTC_TYPE_SEQ)
		{
			//Find the base size
			MtcDLen base_size = mtc_base_type_calc_base_size(iter->type);
				
			fprintf(c_file, 
				"    {\n"
				"        int _i;\n"
				"        MtcSegment sub_seg;\n"
				"        mtc_segment_read_uint32(seg, %s%s.len);\n"
				"        if (mtc_dstream_get_segment(dstream, "
				"%d * %s%s.len, %d * %s%s.len, &sub_seg) < 0)\n"
				"            goto _mtc_fail_%s;\n"
				"        if (! (%s%s.data = (", 
				prefix, iter->parent.name, 
				(int) base_size.n_bytes, prefix, iter->parent.name,
				(int) base_size.n_blocks, prefix, iter->parent.name,
				iter->parent.name, 
				prefix, iter->parent.name);
			mtc_gen_base_type(iter->type, c_file);
			fprintf(c_file, " *) mtc_tryalloc(sizeof(");
			mtc_gen_base_type(iter->type, c_file);
			fprintf(c_file, ") * %s%s.len)))\n"
				"            goto _mtc_fail_%s;\n"
				"        for (_i = 0; _i < %s%s.len; _i++)\n"
				"        {\n"
				"            ",
				prefix, iter->parent.name, 
				iter->parent.name, 
				prefix, iter->parent.name);
			if (mtc_var_code_for_base_read(iter, prefix, "&sub_seg", c_file))
			{
				fprintf(c_file, 
				"            {\n");
				if (mtc_c_base_type_requires_free(iter->type))
				{
					fprintf(c_file, 
				"                for (_i--; _i >= 0; _i--)\n"
				"                {\n"
				"                    ");
					mtc_var_code_for_base_free(iter, prefix, c_file);
					fprintf(c_file, 
				"                }\n");
				}
				fprintf(c_file,
				"                free(%s%s.data);\n"
				"                goto _mtc_fail_%s;\n"
				"            }\n", prefix, iter->parent.name, 
					iter->parent.name);
			}
			fprintf(c_file,
				"        }\n"
				"    }\n");
			
		}
		//Reference
		else if (iter->type.complexity == MTC_TYPE_REF)
		{
			//Find the base size
			MtcDLen base_size = mtc_base_type_calc_base_size(iter->type);
			
			fprintf(c_file, 
				"    {\n"
				"        MtcSegment sub_seg;\n"
				"        char presence;\n"
				"        mtc_segment_read_uchar(seg, presence);\n"
				"        if (presence)\n"
				"        {\n"
				"            if (mtc_dstream_get_segment(dstream, "
				"%d, %d, &sub_seg) < 0)\n"
				"                goto _mtc_fail_%s;\n",
				(int) base_size.n_bytes, 
				(int) base_size.n_blocks, 
				iter->parent.name);
			if (! mtc_c_ref_type_is_baseless(iter->type))
			{
				fprintf(c_file, 
				"            if (! (%s%s = (",
					prefix, iter->parent.name);
				mtc_gen_base_type(iter->type, c_file);
				fprintf(c_file, " *) mtc_tryalloc(sizeof(");
				mtc_gen_base_type(iter->type, c_file);
				fprintf(c_file, "))))\n"
				"            goto _mtc_fail_%s;\n",
					iter->parent.name);
			}
			fprintf(c_file, 
				"            ");
			if (mtc_var_code_for_base_read(iter, prefix, "&sub_seg", c_file))
			{
				fprintf(c_file, 
				"            {\n");
				if (mtc_c_base_type_requires_free(iter->type))
				{
					fprintf(c_file, 
				"                ");
					mtc_var_code_for_base_free(iter, prefix, c_file);
				}
				if (! mtc_c_ref_type_is_baseless(iter->type))
					fprintf(c_file,
				"                free(%s%s);\n",
						prefix, iter->parent.name);
				
				fprintf(c_file,
				"                goto _mtc_fail_%s;\n"
				"            }\n", iter->parent.name);
			}
			fprintf(c_file,
				"        }\n"
				"        else\n"
				"        {\n"
				"            ");
			mtc_var_code_for_ref_null(iter, prefix, c_file);
			fprintf(c_file, 
				"        }\n"
				"    }\n");
		}
		fprintf(c_file, "\n");
	}
}

//writes code to free a variable
void mtc_var_code_for_free
	(MtcSymbolVar *var, const char *prefix, FILE *c_file)
{
	fprintf(c_file, "    //%s\n", var->parent.name);
	
	if (var->type.complexity == MTC_TYPE_NORMAL)
	{
		if (mtc_c_base_type_requires_free(var->type))
		{
			fprintf(c_file, "    ");
			mtc_var_code_for_base_free(var, prefix, c_file);
		}
	}
	else if (var->type.complexity > 0)
	{
		if (mtc_c_base_type_requires_free(var->type))
		{
			fprintf(c_file, 
		"    {\n"
		"        int _i;\n"
		"        for (_i = 0; _i < %d; _i++)\n"
		"        {\n"
		"            ",
				var->type.complexity);
			mtc_var_code_for_base_free(var, prefix, c_file);
			fprintf(c_file, 
		"        }\n"
		"    }\n");
		}	
	}
	else if (var->type.complexity == MTC_TYPE_SEQ)
	{
		if (mtc_c_base_type_requires_free(var->type))
		{
			fprintf(c_file, 
		"    {\n"
		"        int _i;\n"
		"        for (_i = 0; _i < %s%s.len; _i++)\n"
		"        {\n"
		"            ",
				prefix, var->parent.name);
			mtc_var_code_for_base_free(var, prefix, c_file);
			fprintf(c_file, 
		"        }\n"
		"    }\n");
		}
		fprintf(c_file, 
		"    mtc_free(%s%s.data);\n",
			prefix, var->parent.name);
	}
	else if (var->type.complexity == MTC_TYPE_REF)
	{
		int requires_free, baseless;
		
		requires_free = mtc_c_base_type_requires_free(var->type);
		baseless = mtc_c_ref_type_is_baseless(var->type);
		
		if (requires_free || ! baseless)
		{
			fprintf(c_file, "    if (");
			mtc_var_code_ref_test_exp(var, prefix, c_file);
			fprintf(c_file, ")\n"
				"    {\n"
				"        ");
				
			if (requires_free)
			{
				mtc_var_code_for_base_free(var, prefix, c_file);
			}
			if (! baseless)
			{
				fprintf(c_file, "        mtc_free(%s%s);\n",
					prefix, var->parent.name);
			}
			
			fprintf(c_file, "    }\n");
		}
	}
	fprintf(c_file, "\n");
}

//Writes code to deserialize a given list of variables: 
//error handling part
void mtc_var_list_code_for_read_fail
	(MtcSymbolVar *list, const char *prefix, FILE *c_file)
{
	MtcSymbolVar *iter, **start, **end, **iter_rev;
	MtcVector reverser;
	int free_required = 0;
	
	mtc_vector_init(&reverser);
	for (iter = list; iter;
		iter = (MtcSymbolVar *) iter->parent.next)
	{
		mtc_vector_grow(&reverser, sizeof(void *));
		*(mtc_vector_last(&reverser, MtcSymbolVar *)) = iter;
	}
	
	start = mtc_vector_first(&reverser, MtcSymbolVar *);
	end = mtc_vector_last(&reverser, MtcSymbolVar *);
	
	
	for (iter_rev = end; iter_rev >= start; iter_rev--)
	{
		iter = *iter_rev;
		
		if (free_required)
			mtc_var_code_for_free(iter, prefix, c_file);
		
		if (mtc_c_type_read_can_fail(iter->type))
		{
			fprintf(c_file, 
		"    _mtc_fail_%s:\n", iter->parent.name);
			free_required = 1;
		}
	}
	
	mtc_vector_destroy(&reverser);
}

//Writes code to free a given list of variables
void mtc_var_list_code_for_free
	(MtcSymbolVar *list, const char *prefix, FILE *c_file)
{
	MtcSymbolVar *iter;
	for (iter = list; iter;
		iter = (MtcSymbolVar *) iter->parent.next)
	{	
		mtc_var_code_for_free(iter, prefix, c_file);
	}
}

//Writes C code for given structure
void mtc_struct_gen_code
	(MtcSymbolStruct *value, FILE *h_file, FILE *c_file)
{
	MtcSymbolVar *iter;
	MtcDLen base_size;
	int constsize;
	
	//Separator comment
	fprintf(h_file, "//%s\n", value->parent.name);
	fprintf(c_file, "//%s\n", value->parent.name);
	
	//Structure definition
	fprintf(h_file, "typedef struct\n{\n");
	
	for (iter = value->members; iter; 
		iter = (MtcSymbolVar *) iter->parent.next)
	{
		fprintf(h_file, "\t");
		mtc_var_gen(iter, h_file);
		fprintf(h_file, ";\n");
	}
	
	fprintf(h_file, "} %s;\n\n", value->parent.name);
	
	//Size calculation function
	base_size = value->base_size;
	constsize = value->constsize;
	
	if (! constsize)
	{
		//Structure is not of constant size, have to write functions
		fprintf(h_file, 
			"MtcDLen %s__count(%s *value);\n\n",
			value->parent.name, value->parent.name);
		fprintf(c_file, 
			"MtcDLen %s__count(%s *value)\n"
			"{\n"
			"    MtcDLen size = {0, 0};\n\n",
			value->parent.name, value->parent.name);
		
		mtc_var_list_code_for_count(value->members, "value->", c_file);
		
		fprintf(c_file, 
			"\n    return size;\n"
			"}\n\n");	
	}
	
	//Serialization function
	fprintf(h_file, 
		"void %s__write\n"
		"    (%s *value, MtcSegment *seg, MtcDStream *dstream);\n\n",
		value->parent.name, value->parent.name);
	fprintf(c_file, 
		"void %s__write\n"
		"    (%s *value, MtcSegment *seg, MtcDStream *dstream)\n"
		"{\n",
		value->parent.name, value->parent.name);
	
	mtc_var_list_code_for_write(value->members, "value->", c_file);
	
	fprintf(c_file, "}\n\n");
	
	//Deserialization function
	fprintf(h_file, 
		"int %s__read\n"
		"    (%s *value, MtcSegment *seg, MtcDStream *dstream);\n\n",
		value->parent.name, value->parent.name);
	fprintf(c_file, 
		"int %s__read\n"
		"    (%s *value, MtcSegment *seg, MtcDStream *dstream)\n"
		"{\n",
		value->parent.name, value->parent.name);
	
	mtc_var_list_code_for_read
		(value->members, "value->", c_file);
	fprintf(c_file, "\n    return 0;\n\n");
	mtc_var_list_code_for_read_fail(value->members, "value->", c_file);
	fprintf(c_file, "\n    return -1;\n}\n\n");
	
	//Function to free the structure
	fprintf(h_file, 
		"void %s__free(%s *value);\n\n",
		value->parent.name, value->parent.name);
	fprintf(c_file, 
		"void %s__free(%s *value)\n"
		"{\n",
		value->parent.name, value->parent.name);
	
	mtc_var_list_code_for_free(value->members, "value->", c_file);
	
	fprintf(c_file, "}\n\n");
	
	//Function to serialize a structure into a message
	fprintf(h_file, 
		"MtcMsg *%s__serialize(%s *value);\n\n",
		value->parent.name, value->parent.name);
	fprintf(c_file, 
		"MtcMsg *%s__serialize(%s *value)\n"
		"{\n"
		"    MtcSegment seg;\n"
		"    MtcDStream dstream;\n"
		"    MtcDLen dlen = {%d, %d};\n"
		"    MtcMsg *msg;\n"
		"    \n",
		value->parent.name, value->parent.name, 
		(int) base_size.n_bytes, (int) base_size.n_blocks);
	if (! constsize)
	{
		fprintf(c_file, 
		"    //Size computation\n"
		"    {\n"
		"        MtcDLen dynamic;\n"
		"        dynamic = %s__count(value);\n"
		"        dlen.n_bytes += dynamic.n_bytes;\n"
		"        dlen.n_blocks += dynamic.n_blocks;\n"
		"    }\n"
		"    \n",
		value->parent.name);
	}
	fprintf(c_file, 
		"    msg = mtc_msg_new(dlen.n_bytes, dlen.n_blocks);\n"
		"    \n"
		"    mtc_msg_iter(msg, &dstream);\n"
		"    mtc_dstream_get_segment(&dstream, %d, %d, &seg);\n"
		"    \n"
		"    %s__write(value, &seg, &dstream);\n"
		"    \n"
		"    return msg;\n"
		"}\n\n",
		(int) base_size.n_bytes, (int) base_size.n_blocks, 
		value->parent.name);
	
	//Function to deserialize a message to get back structure
	fprintf(h_file, 
		"int %s__deserialize(MtcMsg *msg, %s *value);\n\n",
		value->parent.name, value->parent.name);
	fprintf(c_file, 
		"int %s__deserialize(MtcMsg *msg, %s *value)\n"
		"{\n"
		"    MtcSegment seg;\n"
		"    MtcDStream dstream;\n"
		"    \n"
		"    mtc_msg_iter(msg, &dstream);\n"
		"    if (mtc_dstream_get_segment(&dstream, %d, %d, &seg) < 0)\n"
		"        goto _mtc_return;\n"
		"    \n"
		"    if (%s__read(value, &seg, &dstream) < 0)\n"
		"        goto _mtc_return;\n"
		"    \n"
		"    if (! mtc_dstream_is_empty(&dstream))\n"
		"        goto _mtc_destroy_n_return;\n"
		"    \n"
		"    return 0;\n"
		"    \n"
		"_mtc_destroy_n_return:\n"
		"    %s__free(value);\n"
		"_mtc_return:\n"
		"    return -1;\n"
		"}\n\n",
		value->parent.name, value->parent.name,
		(int) base_size.n_bytes, (int) base_size.n_blocks,
		value->parent.name,
		value->parent.name);
}
