/* types.c
 * Making data portable
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
/* ********************************************************************/ 
 
/* NOTE:
 * 1. Little endian will be the byte order for internet domain links.
 * 2. Signed integers will be represented in two's complement form.
 */

#include "common.h"

#include <limits.h>
#include <math.h>


//byte order conversion
#ifndef MTC_UINT16_LITTLE_ENDIAN
//Converts a 16-bit integer from native byte order to little endian
//byte order and vice versa.
uint16_t mtc_uint16_swap_native_le(uint16_t val)
{
	uint16_t res;
	char *val_dec = (char *) &val;
	char *res_dec = (char *) &res;
	
	res_dec[0] = val_dec[1];
	res_dec[1] = val_dec[0];
	
	return res;
}
#endif

#ifndef MTC_UINT32_LITTLE_ENDIAN
//Converts a 32-bit integer from native byte order to little endian
//byte order.
uint32_t mtc_uint32_to_le(uint32_t val)
{
	uint32_t res;
	char *val_dec = (char *) &val;
	char *res_dec = (char *) &res;
	
	mtc_uint32_copy_to_le(res_dec, val_dec);
	
	return res;
}

//Converts a 32-bit integer from little endian byte order to native
//byte order.
uint32_t mtc_uint32_from_le(uint32_t val)
{
	uint32_t res;
	char *val_dec = (char *) &val;
	char *res_dec = (char *) &res;
	
	mtc_uint32_copy_from_le(res_dec, val_dec);
	
	return res;
}

#endif

#ifndef MTC_UINT64_LITTLE_ENDIAN
//Converts a 64-bit integer from native byte order to little endian
//byte order.
uint64_t mtc_uint64_to_le(uint64_t val)
{
	uint64_t res;
	char *val_dec = (char *) &val;
	char *res_dec = (char *) &res;
	
	mtc_uint64_copy_to_le(res_dec, val_dec);
	
	return res;
}

//Converts a 64-bit integer from little endian byte order to native
//byte order.
uint64_t mtc_uint64_from_le(uint64_t val)
{
	uint64_t res;
	char *val_dec = (char *) &val;
	char *res_dec = (char *) &res;
	
	mtc_uint64_copy_from_le(res_dec, val_dec);
	
	return res;
}

#endif

//Two's complement conversions

#ifndef MTC_INT_2_COMPLEMENT
//Converts a signed char to two's complement form
unsigned char mtc_char_to_2_complement(char val)
{
	if (val < 0)
	{
		unsigned char res;
		res = -val;   //< Take absolute value
		res = ~ res;  //< one's complement
		res += 1;     //< two's complement
		return res;
	}
	return (unsigned char) val;
}
//Converts a 16-bit signed integer to two's complement form
uint16_t mtc_int16_to_2_complement(int16_t val)
{
	if (val < 0)
	{
		uint16_t res;
		res = -val;   //< Take absolute value
		res = ~ res;  //< one's complement
		res += 1;     //< two's complement
		return res;
	}
	return (uint16_t) val;
}

//Converts a 32-bit signed integer to two's complement form
uint32_t mtc_int32_to_2_complement(int32_t val)
{
	if (val < 0)
	{
		uint32_t res;
		res = -val;   //< Take absolute value
		res = ~ res;  //< one's complement
		res += 1;     //< two's complement
		return res;
	}
	return (uint32_t) val;
}

//Converts a 64-bit signed integer to two's complement form
uint64_t mtc_int64_to_2_complement(int64_t val)
{
	if (val < 0)
	{
		uint64_t res;
		res = -val;   //< Take absolute value
		res = ~ res;  //< one's complement
		res += 1;     //< two's complement
		return res;
	}
	return (uint64_t) val;
}

//Converts a signed char in two's complement form to native form
char mtc_char_from_2_complement(unsigned char val)
{
	//Check for negative value
	if (val & (1 << 7))
	{
		char res;
		//Follow inverse operations
		val -= 1;
		val = ~val;
		res = -val;
		return res;
	}
	return (char) val;
}

//Converts a 16-bit integer in two's complement form to native form
int16_t mtc_int16_from_2_complement(uint16_t val)
{
	//Check for negative value
	if (val & (1 << 15))
	{
		int16_t res;
		//Follow inverse operations
		val -= 1;
		val = ~val;
		res = -val;
		return res;
	}
	return (int16_t) val;
}

//Converts a 32-bit integer in two's complement form to native form
int32_t mtc_int32_from_2_complement(uint32_t val)
{
	//Check for negative value
	if (val & (1 << 31))
	{
		int32_t res;
		//Follow inverse operations
		val -= 1;
		val = ~val;
		res = -val;
		return res;
	}
	return (int32_t) val;
}

//Converts a 64-bit integer in two's complement form to native form
int64_t mtc_int64_from_2_complement(uint64_t val)
{
	//Check for negative value
	if (val & (1LL << 63))
	{
		int64_t res;
		//Follow inverse operations
		val -= 1;
		val = ~val;
		res = -val;
		return res;
	}
	return (int64_t) val;
}
#endif

//Returns a 32-bit integer representing the given float value.
uint32_t mtc_float_to_flt32(float v)
{
	int float_type;
	uint32_t res;
	
	//Find the type of floating point
	float_type = fpclassify(v); 
	
	//Check for NaN
	if (float_type == FP_NAN)
	{
		res = 0x7fffffff;
	}
	//Zero
	else if (float_type == FP_ZERO)
	{
		res = 0x00000000;
	}
	else
	{
		int sign;
		
		//Extract sign
		sign = signbit(v);
		if (sign)
			sign = 1;
		
		//+/- infinity
		if (float_type == FP_INFINITE)
		{
			res = 0x7f800000;
		}
		else
		{
			//Normal number
			int exponent;
			float fraction;
			
			//Decompose the number
			fraction = frexpf(v, &exponent);
			
			
			//Approximate to infinity in case number is too large
			if (exponent > 128)
			{
				res = 0x7f800000;
			}
			else
			{
				//Make fraction positive
				if (sign)
					fraction = -fraction;
				
				//Extract the mantissa
				if (exponent < -125)
				{
					//Subnormal
					fraction *= (1 << (24 + exponent + 125));
					res = fraction;
					//Exponent area is already zero, no need to attach it
				}
				else
				{
					//Normal
					fraction *= (1 << 24);
					res = (uint32_t) fraction;
					//Remove the preceeding 1
					res &= 0x007fffff;
					//Attach the exponent
					res |= ((exponent + 126) << 23);
				}
			}
		}
		
		//Add sign bit
		res |= (sign << 31);
	}
	
	//Change byte order and return
	return res;
}

//Retrieves float value from 32-bit integer. 
float mtc_float_from_flt32(uint32_t v)
{
	float res;
	uint32_t exp_region, mantissa;
	int sign;
	int exponent;
	
	//Dissect it
	sign = (v & 0x80000000) ? -1 : 1;
	exp_region = v & 0x7f800000;
	mantissa = v & 0x007fffff;
	
	//Infinity and NaN
	if (exp_region == 0x7f800000)
	{
		if (mantissa)
		{
			//NaN
#ifdef NAN
			return NAN;
#else
			//Is this portable?
			return 0.0 / 0.0;
#endif
		}
		else
		{
			//infinity
			return sign * HUGE_VALF;
		}
	}
	//Subnormal numbers
	else if (! exp_region)
	{
		res = ldexpf((float) mantissa, -149);
		res *= sign;
	}
	else
	{
		//Find mantissa and exponent
		exponent = exp_region >> 23;
		mantissa |= 1 << 23;
		//Assemble them together
		res = ldexpf((float) mantissa, exponent - 150);
		res *= sign;
	}
	
	//return
	return res;
}

//Returns a 32-bit integer representing the given double value.
uint32_t mtc_double_to_flt32(double v)
{
	int float_type;
	uint32_t res;
	
	//Find the type of floating point
	float_type = fpclassify(v); 
	
	//Check for NaN
	if (float_type == FP_NAN)
	{
		res = 0x7fffffff;
	}
	//Zero
	else if (float_type == FP_ZERO)
	{
		res = 0x00000000;
	}
	else
	{
		int sign;
		
		//Extract sign
		sign = signbit(v);
		if (sign)
			sign = 1;
		
		//+/- infinity
		if (float_type == FP_INFINITE)
		{
			res = 0x7f800000;
		}
		else
		{
			//Normal number
			int exponent;
			double fraction;
			
			//Decompose the number
			fraction = frexp(v, &exponent);
			
			
			//Approximate to infinity in case number is too large
			if (exponent > 128)
			{
				res = 0x7f800000;
			}
			else
			{
				//Make fraction positive
				if (sign)
					fraction = -fraction;
				
				//Extract the mantissa
				if (exponent < -125)
				{
					//Subnormal
					fraction *= (1 << (24 + exponent + 125));
					res = fraction;
					//Exponent area is already zero, no need to attach it
				}
				else
				{
					//Normal
					fraction *= (1 << 24);
					res = (uint32_t) fraction;
					//Remove the preceeding 1
					res &= 0x007fffff;
					//Attach the exponent
					res |= ((exponent + 126) << 23);
				}
			}
		}
		
		//Add sign bit
		res |= (sign << 31);
	}
	
	//Change byte order and return
	return res;
}

//Retrieves double value from 32-bit integer. 
double mtc_double_from_flt32(uint32_t v)
{
	double res;
	uint32_t exp_region, mantissa;
	int sign;
	int exponent;
	
	
	//Dissect it
	sign = (v & 0x80000000) ? -1 : 1;
	exp_region = v & 0x7f800000;
	mantissa = v & 0x007fffff;
	
	//Infinity and NaN
	if (exp_region == 0x7f800000)
	{
		if (mantissa)
		{
			//NaN
#ifdef NAN
			return NAN;
#else
			//Is this portable?
			return 0.0 / 0.0;
#endif
		}
		else
		{
			//infinity
			return sign * HUGE_VALF;
		}
	}
	//Subnormal numbers
	else if (! exp_region)
	{
		res = ldexp((double) mantissa, -149);
		res *= sign;
	}
	else
	{
		//Find mantissa and exponent
		exponent = exp_region >> 23;
		mantissa |= 1 << 23;
		//Assemble them together
		res = ldexp((double) mantissa, exponent - 150);
		res *= sign;
	}
	
	//return
	return res;
}

//Returns a 32-bit integer representing the given double value.
uint64_t mtc_double_to_flt64(double v)
{
	int float_type;
	uint64_t res;
	
	//Find the type of floating point
	float_type = fpclassify(v); 
	
	//Check for NaN
	if (float_type == FP_NAN)
	{
		res = 0x7fffffffffffffffLL;
	}
	//Zero
	else if (float_type == FP_ZERO)
	{
		res = 0x0000000000000000LL;
	}
	else
	{
		int sign;
		
		//Extract sign
		sign = signbit(v);
		if (sign)
			sign = 1;
		
		//+/- infinity
		if (float_type == FP_INFINITE)
		{
			res = 0x7ff0000000000000LL;
		}
		else
		{
			//Normal number
			int exponent;
			double fraction;
			
			//Decompose the number
			fraction = frexp(v, &exponent);
			
			
			//Approximate to infinity in case number is too large
			if (exponent > 1024)
			{
				res = 0x7ff0000000000000LL;
			}
			else
			{
				//Make fraction positive
				if (sign)
					fraction = -fraction;
				
				//Extract the mantissa
				if (exponent < -1021)
				{
					//Subnormal
					fraction *= (1LL << (53 + exponent + 1021));
					res = fraction;
					//Exponent area is already zero, no need to attach it
				}
				else
				{
					//Normal
					fraction *= (1LL << 53);
					res = (uint64_t) fraction;
					//Remove the preceeding 1
					res &= 0x000fffffffffffffLL;
					//Attach the exponent
					res |= ((uint64_t) (exponent + 1022) << 52);
				}
			}
		}
		
		//Add sign bit
		res |= ((uint64_t) sign << 63);
	}
	
	//return
	return res;
}

//Retrieves double value from 64-bit integer. 
double mtc_double_from_flt64(uint64_t v)
{
	double res;
	uint64_t exp_region, mantissa;
	int sign;
	int exponent;
	
	//Dissect it
	sign = (v & 0x8000000000000000LL) ? -1 : 1;
	exp_region = v & 0x7ff0000000000000LL;
	mantissa = v & 0x000fffffffffffffLL;
	
	//Infinity and NaN
	if (exp_region == 0x7ff0000000000000LL)
	{
		if (mantissa)
		{
			//NaN
#ifdef NAN
			return NAN;
#else
			//Is this portable?
			return 0.0 / 0.0;
#endif
		}
		else
		{
			//infinity
			return sign * HUGE_VALF;
		}
	}
	//Subnormal numbers
	else if (! exp_region)
	{
		res = ldexp((double) mantissa, -1074);
		res *= sign;
	}
	else
	{
		//Find mantissa and exponent
		exponent = exp_region >> 52;
		mantissa |= 1LL << 52;
		//Assemble them together
		res = ldexp((double) mantissa, exponent - 1075);
		res *= sign;
	}
	
	//return
	return res;
}

//Finds the type of floating point value in given 32-bit integer.
MtcFltType mtc_flt32_classify(uint32_t val)
{
	switch (val)
	{
	case 0x00000000:	
		return MTC_FLT_ZERO;
	case 0x7f800000:
		return MTC_FLT_INFINITE;
	case 0xff800000:
		return MTC_FLT_NEG_INFINITE;
	default:
		if ((val & 0x7f800000) == 0x7f800000)
			return MTC_FLT_NAN;
		return MTC_FLT_NORMAL;
	}
}

//Finds the type of floating point value in given 64-bit integer.
MtcFltType mtc_flt64_classify(uint64_t val)
{
	switch (val)
	{
	case 0x0000000000000000LL:	
		return MTC_FLT_ZERO;
	case 0x7ff0000000000000LL:
		return MTC_FLT_INFINITE;
	case 0xfff0000000000000LL:
		return MTC_FLT_NEG_INFINITE;
	default:
		if ((val & 0x7ff0000000000000LL) == 0x7ff0000000000000LL)
			return MTC_FLT_NAN;
		return MTC_FLT_NORMAL;
	}
}

