/*
Developed by ESN, an Electronic Arts Inc. studio. 
Copyright (c) 2014, Electronic Arts Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of ESN, Electronic Arts Inc. nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS INC. BE LIABLE 
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Portions of code from MODP_ASCII - Ascii transformations (upper/lower, etc)
http://code.google.com/p/stringencoders/
Copyright (c) 2007	Nick Galbreath -- nickg [at] modp [dot] com. All rights reserved.

Numeric decoder derived from from TCL library
http://www.opensource.apple.com/source/tcl/tcl-14/tcl/license.terms
* Copyright (c) 1988-1993 The Regents of the University of California.
* Copyright (c) 1994 Sun Microsystems, Inc.
*/

#include "ujson.h"
#include <math.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

struct DecoderState
{
	const char *start;
	const char *end;
	char *escStart;
	char *escEnd;
	int escHeap;
	int lastType;
	JSUINT32 objDepth;
	void *prv;
	JSONObjectDecoder *dec;
};

JSOBJ decode_any( struct DecoderState *ds);
typedef JSOBJ (*PFN_DECODER)( struct DecoderState *ds);

static JSOBJ SetError( struct DecoderState *ds, int offset, const char *message)
{
	ds->dec->errorOffset = ds->start + offset;
	ds->dec->errorStr = (const char *) message;
	return NULL;
}

static double createDouble(double intNeg, double intValue, double frcValue, int frcDecimalCount)
{
	static const double g_pow10[] = {1.0, 0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001,0.0000001, 0.00000001, 0.000000001, 0.0000000001, 0.00000000001, 0.000000000001, 0.0000000000001, 0.00000000000001, 0.000000000000001};
	return (intValue + (frcValue * g_pow10[frcDecimalCount])) * intNeg;
}

static JSOBJ decodePreciseFloat(struct DecoderState *ds)
{
	char *end;
	double value;
	errno = 0;

	value = strtod(ds->start, &end);

	if (errno == ERANGE)
	{
		return SetError(ds, -1, "Range error when decoding numeric as double");
	}

	ds->start = end;
	return ds->dec->newDouble(ds->prv, value);
}

static JSOBJ decode_numeric (struct DecoderState *ds)
{
	int intNeg = 1;
	int mantSize = 0;
	JSUINT64 intValue;
	JSUINT64 prevIntValue;
	int chr;
	int decimalCount = 0;
	double frcValue = 0.0;
	double expNeg;
	double expValue;
	const char *offset = ds->start;

	JSUINT64 overflowLimit = LLONG_MAX;

	if (*(offset) == '-')
	{
		offset ++;
		intNeg = -1;
		overflowLimit = LLONG_MIN;
	}

	// Scan integer part
	intValue = 0;

	while (1)
	{
		chr = (int) (unsigned char) *(offset);

		switch (chr)
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				//PERF: Don't do 64-bit arithmetic here unless we know we have to
				prevIntValue = intValue;
				intValue = intValue * 10ULL + (JSLONG) (chr - 48);

				if (intNeg == 1 && prevIntValue > intValue)
				{
					return SetError(ds, -1, "Value is too big!");
				}
				else if (intNeg == -1 && intValue > overflowLimit)
				{
					return SetError(ds, -1, overflowLimit == LLONG_MAX ? "Value is too big!" : "Value is too small");
				}

				offset ++;
				mantSize ++;
				break;
			}
			case '.':
			{
				offset ++;
				goto DECODE_FRACTION;
				break;
			}
			case 'e':
			case 'E':
			{
				offset ++;
				goto DECODE_EXPONENT;
				break;
			}

			default:
			{
				goto BREAK_INT_LOOP;
				break;
			}
		}
	}

BREAK_INT_LOOP:

	ds->lastType = JT_INT;
	ds->start = offset;

	if (intNeg == 1 && (intValue & 0x8000000000000000ULL) != 0)
	{
		return ds->dec->newUnsignedLong(ds->prv, intValue);
	}
	else if ((intValue >> 31))
	{
		return ds->dec->newLong(ds->prv, (JSINT64) (intValue * (JSINT64) intNeg));
	}
	else
	{
		return ds->dec->newInt(ds->prv, (JSINT32) (intValue * intNeg));
	}

DECODE_FRACTION:

	if (ds->dec->preciseFloat)
	{
		return decodePreciseFloat(ds);
	}

	// Scan fraction part
	frcValue = 0.0;
	for (;;)
	{
		chr = (int) (unsigned char) *(offset);

		switch (chr)
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				if (decimalCount < JSON_DOUBLE_MAX_DECIMALS)
				{
					frcValue = frcValue * 10.0 + (double) (chr - 48);
					decimalCount ++;
				}
				offset ++;
				break;
			}
			case 'e':
			case 'E':
			{
				offset ++;
				goto DECODE_EXPONENT;
				break;
			}
			default:
			{
				goto BREAK_FRC_LOOP;
			}
		}
	}

BREAK_FRC_LOOP:
	//FIXME: Check for arithemtic overflow here
	ds->lastType = JT_DOUBLE;
	ds->start = offset;
	return ds->dec->newDouble (ds->prv, createDouble( (double) intNeg, (double) intValue, frcValue, decimalCount));

DECODE_EXPONENT:
	if (ds->dec->preciseFloat)
	{
		return decodePreciseFloat(ds);
	}

	expNeg = 1.0;

	if (*(offset) == '-')
	{
		expNeg = -1.0;
		offset ++;
	}
	else
	if (*(offset) == '+')
	{
		expNeg = +1.0;
		offset ++;
	}

	expValue = 0.0;

	for (;;)
	{
		chr = (int) (unsigned char) *(offset);

		switch (chr)
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				expValue = expValue * 10.0 + (double) (chr - 48);
				offset ++;
				break;
			}
			default:
			{
				goto BREAK_EXP_LOOP;
			}
		}
	}

BREAK_EXP_LOOP:
	//FIXME: Check for arithemtic overflow here
	ds->lastType = JT_DOUBLE;
	ds->start = offset;
	return ds->dec->newDouble (ds->prv, createDouble( (double) intNeg, (double) intValue , frcValue, decimalCount) * pow(10.0, expValue * expNeg));
}

static JSOBJ decode_true ( struct DecoderState *ds)
{
	const char *offset = ds->start;
	offset ++;

	if (*(offset++) != 'r')
		goto SETERROR;
	if (*(offset++) != 'u')
		goto SETERROR;
	if (*(offset++) != 'e')
		goto SETERROR;

	ds->lastType = JT_TRUE;
	ds->start = offset;
	return ds->dec->newTrue(ds->prv);

SETERROR:
	return SetError(ds, -1, "Unexpected character found when decoding 'true'");
}

static JSOBJ decode_false ( struct DecoderState *ds)
{
	const char *offset = ds->start;
	offset ++;

	if (*(offset++) != 'a')
		goto SETERROR;
	if (*(offset++) != 'l')
		goto SETERROR;
	if (*(offset++) != 's')
		goto SETERROR;
	if (*(offset++) != 'e')
		goto SETERROR;

	ds->lastType = JT_FALSE;
	ds->start = offset;
	return ds->dec->newFalse(ds->prv);

SETERROR:
	return SetError(ds, -1, "Unexpected character found when decoding 'false'");
}

static JSOBJ decode_null ( struct DecoderState *ds)
{
	const char *offset = ds->start;
	offset ++;

	if (*(offset++) != 'u')
		goto SETERROR;
	if (*(offset++) != 'l')
		goto SETERROR;
	if (*(offset++) != 'l')
		goto SETERROR;

	ds->lastType = JT_NULL;
	ds->start = offset;
	return ds->dec->newNull(ds->prv);

SETERROR:
	return SetError(ds, -1, "Unexpected character found when decoding 'null'");
}

static void SkipWhitespace(struct DecoderState *ds)
{
	const char *offset = ds->start;

	for (;;)
	{
		switch (*offset)
		{
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				offset ++;
				break;

			default:
				ds->start = offset;
				return;
		}
	}
}

enum DECODESTRINGSTATE
{
	DS_ISNULL = 0x32,
	DS_ISQUOTE,
	DS_ISESCAPE,
	DS_UTFLENERROR,

};

static const JSUINT8 g_decoderLookup[256] =
{
	/* 0x00 */ DS_ISNULL, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0x10 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0x20 */ 1, 1, DS_ISQUOTE, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0x30 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0x40 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0x50 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, DS_ISESCAPE, 1, 1, 1,
	/* 0x60 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0x70 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0x80 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0x90 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0xa0 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0xb0 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0xc0 */ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	/* 0xd0 */ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	/* 0xe0 */ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	/* 0xf0 */ 4, 4, 4, 4, 4, 4, 4, 4, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR, DS_UTFLENERROR,
};

static JSOBJ decode_string ( struct DecoderState *ds)
{
	JSUTF16 sur[2] = { 0 };
	int iSur = 0;
	char *escOffset;
	char *escStart;
	size_t escLen = (ds->escEnd - ds->escStart);
	const JSUINT8 *inputOffset;
	JSUINT8 oct;
	//JSUTF32 ucs;
	ds->lastType = JT_INVALID;
	ds->start ++;

	if ( (size_t) (ds->end - ds->start) > escLen)
	{
		size_t newSize = (ds->end - ds->start);

		if (ds->escHeap)
		{
			if (newSize > (SIZE_MAX / sizeof(char)))
			{
				return SetError(ds, -1, "Could not reserve memory block");
			}
			escStart = (char *)ds->dec->realloc(ds->escStart, newSize * sizeof(char));
			if (!escStart)
			{
				ds->dec->free(ds->escStart);
				return SetError(ds, -1, "Could not reserve memory block");
			}
			ds->escStart = escStart;
		}
		else
		{
			char *oldStart = ds->escStart;
			if (newSize > (SIZE_MAX / sizeof(char)))
			{
				return SetError(ds, -1, "Could not reserve memory block");
			}
			ds->escStart = (char *) ds->dec->malloc(newSize * sizeof(char));
			if (!ds->escStart)
			{
				return SetError(ds, -1, "Could not reserve memory block");
			}
			ds->escHeap = 1;
			memcpy(ds->escStart, oldStart, escLen * sizeof(char));
		}

		ds->escEnd = ds->escStart + newSize;
	}

	escOffset = ds->escStart;
	inputOffset = (const JSUINT8 *) ds->start;

	for (;;)
	{
		switch (g_decoderLookup[(JSUINT8)(*inputOffset)])
		{
			case DS_ISNULL:
			{
				return SetError(ds, -1, "Unmatched ''\"' when when decoding 'string'");
			}
			case DS_ISQUOTE:
			{
				ds->lastType = JT_UTF8;
				inputOffset ++;
				ds->start += ( (const char *) inputOffset - (ds->start));
				return ds->dec->newString(ds->prv, ds->escStart, escOffset);
			}
			case DS_UTFLENERROR:
			{
				return SetError (ds, -1, "Invalid UTF-8 sequence length when decoding 'string'");
			}
			case DS_ISESCAPE:
				inputOffset ++;
				switch (*inputOffset)
				{
					case '\\': *(escOffset++) = L'\\'; inputOffset++; continue;
					case '\"': *(escOffset++) = L'\"'; inputOffset++; continue;
					case '/':	*(escOffset++) = L'/';	inputOffset++; continue;
					case 'b':	*(escOffset++) = L'\b'; inputOffset++; continue;
					case 'f':	*(escOffset++) = L'\f'; inputOffset++; continue;
					case 'n':	*(escOffset++) = L'\n'; inputOffset++; continue;
					case 'r':	*(escOffset++) = L'\r'; inputOffset++; continue;
					case 't':	*(escOffset++) = L'\t'; inputOffset++; continue;

					case 'u':
					{
						inputOffset ++;

						for (int i = 0; i < 4; i ++)
						{
							switch (*inputOffset)
							{
								case '\0': return SetError (ds, -1, "Unterminated unicode escape sequence when decoding 'string'");
								default: return SetError (ds, -1, "Unexpected character in unicode escape sequence when decoding 'string'");

								case '0':
								case '1':
								case '2':
								case '3':
								case '4':
								case '5':
								case '6':
								case '7':
								case '8':
								case '9':
									sur[iSur] = (sur[iSur] << 4) + (JSUTF16) (*inputOffset - '0');
									break;

								case 'a':
								case 'b':
								case 'c':
								case 'd':
								case 'e':
								case 'f':
									sur[iSur] = (sur[iSur] << 4) + 10 + (JSUTF16) (*inputOffset - 'a');
									break;

								case 'A':
								case 'B':
								case 'C':
								case 'D':
								case 'E':
								case 'F':
									sur[iSur] = (sur[iSur] << 4) + 10 + (JSUTF16) (*inputOffset - 'A');
									break;
							}

							inputOffset ++;
						}

						if (iSur == 0)
						{
							if((sur[iSur] & 0xfc00) == 0xd800)
							{
								// First of a surrogate pair, continue parsing
								iSur ++;
								break;
							}
							(*escOffset++) = (char) sur[iSur];
							iSur = 0;
						}
						else
						{
							// Decode pair
							if ((sur[1] & 0xfc00) != 0xdc00)
							{
								return SetError (ds, -1, "Unpaired high surrogate when decoding 'string'");
							}
#if WCHAR_MAX == 0xffff
							(*escOffset++) = (char) sur[0];
							(*escOffset++) = (char) sur[1];
#else
							(*escOffset++) = (char) 0x10000 + (((sur[0] - 0xd800) << 10) | (sur[1] - 0xdc00));
#endif
							iSur = 0;
						}
					break;
				}

				case '\0': return SetError(ds, -1, "Unterminated escape sequence when decoding 'string'");
				default: return SetError(ds, -1, "Unrecognized escape sequence when decoding 'string'");
			}
			break;

			case 1:
			{
				*(escOffset++) = (char) (*inputOffset++);
				break;
			}

			case 2:
			{
				JSUTF32 ucs = (*inputOffset++) & 0x1f;
				ucs <<= 6;
				if (((*inputOffset) & 0x80) != 0x80)
				{
					return SetError(ds, -1, "Invalid octet in UTF-8 sequence when decoding 'string'");
				}
				ucs |= (*inputOffset++) & 0x3f;
				if (ucs < 0x80) return SetError (ds, -1, "Overlong 2 byte UTF-8 sequence detected when decoding 'string'");
				*(escOffset++) = (char) ucs;
				break;
			}

			case 3:
			{
				JSUTF32 ucs = 0;
				ucs |= (*inputOffset++) & 0x0f;

				for (int i = 0; i < 2; i ++)
				{
					ucs <<= 6;
					oct = (*inputOffset++);

					if ((oct & 0x80) != 0x80)
					{
						return SetError(ds, -1, "Invalid octet in UTF-8 sequence when decoding 'string'");
					}

					ucs |= oct & 0x3f;
				}

				if (ucs < 0x800) return SetError (ds, -1, "Overlong 3 byte UTF-8 sequence detected when encoding string");
				*(escOffset++) = (char) ucs;
				break;
			}

			case 4:
			{
				JSUTF32 ucs = 0;
				ucs |= (*inputOffset++) & 0x07;

				for (int index = 0; index < 3; index ++)
				{
					ucs <<= 6;
					oct = (*inputOffset++);

					if ((oct & 0x80) != 0x80)
					{
						return SetError(ds, -1, "Invalid octet in UTF-8 sequence when decoding 'string'");
					}

					ucs |= oct & 0x3f;
				}

				if (ucs < 0x10000) return SetError (ds, -1, "Overlong 4 byte UTF-8 sequence detected when decoding 'string'");

#if WCHAR_MAX == 0xffff
				if (ucs >= 0x10000)
				{
					ucs -= 0x10000;
					*(escOffset++) = (char) (ucs >> 10) + 0xd800;
					*(escOffset++) = (char) (ucs & 0x3ff) + 0xdc00;
				}
				else
				{
					*(escOffset++) = (char) ucs;
				}
#else
				*(escOffset++) = (char) ucs;
#endif
				break;
			}
		}
	}
}

static JSOBJ decode_array(struct DecoderState *ds)
{
	JSOBJ itemValue;
	JSOBJ newObj;
	int len;
	ds->objDepth++;
	if (ds->objDepth > JSON_MAX_OBJECT_DEPTH) {
		return SetError(ds, -1, "Reached object decoding depth limit");
	}

	newObj = ds->dec->newArray(ds->prv);
	len = 0;

	ds->lastType = JT_INVALID;
	ds->start ++;

	for (;;)
	{
		SkipWhitespace(ds);

		if ((*ds->start) == ']'){
			ds->objDepth--;
			if (len == 0)
			{
				ds->start ++;
				return newObj;
			}
		printf("release object\n"); 
			ds->dec->releaseObject(ds->prv, newObj);
			return SetError(ds, -1, "Unexpected character found when decoding array value (1)");
		}

		itemValue = decode_any(ds);

		if (itemValue == NULL)
		{
			ds->dec->releaseObject(ds->prv, newObj);
			return NULL;
		}
	
		ds->dec->arrayAddItem (ds->prv, newObj, itemValue);

		SkipWhitespace(ds);

		switch (*(ds->start++))
		{
		case ']':{
			ds->objDepth--;
			ds->dec->releaseObject(ds->prv, newObj);
			return newObj;
		}
		case ',':
			break;

		default:
			ds->dec->releaseObject(ds->prv, newObj);
			return SetError(ds, -1, "Unexpected character found when decoding array value (2)");
		}

		len ++;
	}
}

static JSOBJ decode_object( struct DecoderState *ds)
{
	JSOBJ itemName;
	JSOBJ itemValue;
	JSOBJ newObj;

	ds->objDepth++;
	if (ds->objDepth > JSON_MAX_OBJECT_DEPTH) {
		return SetError(ds, -1, "Reached object decoding depth limit");
	}

	newObj = ds->dec->newObject(ds->prv);

	ds->start ++;

	for (;;)
	{
		SkipWhitespace(ds);

		if ((*ds->start) == '}')
		{
			ds->objDepth--;
			ds->start ++;
			ds->dec->releaseObject(ds->prv, newObj);
			return newObj;
		}

		ds->lastType = JT_INVALID;
		itemName = decode_any(ds);

		if (itemName == NULL)
		{
			ds->dec->releaseObject(ds->prv, newObj);
			return NULL;
		}

		if (ds->lastType != JT_UTF8)
		{
			ds->dec->releaseObject(ds->prv, newObj);
			ds->dec->releaseObject(ds->prv, itemName);
			return SetError(ds, -1, "Key name of object must be 'string' when decoding 'object'");
		}

		SkipWhitespace(ds);

		if (*(ds->start++) != ':')
		{
			ds->dec->releaseObject(ds->prv, newObj);
			ds->dec->releaseObject(ds->prv, itemName);
			return SetError(ds, -1, "No ':' found when decoding object value");
		}

		SkipWhitespace(ds);

		itemValue = decode_any(ds);

		if (itemValue == NULL)
		{
			ds->dec->releaseObject(ds->prv, newObj);
			ds->dec->releaseObject(ds->prv, itemName);
			return NULL;
		}

		ds->dec->objectAddKey (ds->prv, newObj, itemName, itemValue);

		SkipWhitespace(ds);

		switch (*(ds->start++))
		{
			case '}':
			{
				ds->objDepth--;
				ds->dec->releaseObject(ds->prv, newObj);
				return newObj;
			}
			case ',':
				break;

			default:
				ds->dec->releaseObject(ds->prv, newObj);
				return SetError(ds, -1, "Unexpected character in found when decoding object value");
		}
	}
}

JSOBJ decode_any(struct DecoderState *ds)
{
	for (;;)
	{
		switch (*ds->start)
		{
			case '\"':
				return decode_string (ds);
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '-':
				return decode_numeric (ds);

			case '[': return decode_array (ds);
			case '{': return decode_object (ds);
			case 't': return decode_true (ds);
			case 'f': return decode_false (ds);
			case 'n': return decode_null (ds);

			case ' ':
			case '\t':
			case '\r':
			case '\n':
				// White space
				ds->start ++;
				break;

			default:
				return SetError(ds, -1, "Expected object or value");
		}
	}
}

JSOBJ JSON_DecodeObject(JSONObjectDecoder *dec, const char *buffer, size_t cbBuffer)
{
	/*
	FIXME: Base the size of escBuffer of that of cbBuffer so that the unicode escaping doesn't run into the wall each time */
	struct DecoderState ds;
	char escBuffer[(JSON_MAX_STACK_BUFFER_SIZE / sizeof(char))];
	JSOBJ ret;

	ds.start = (const char *) buffer;
	ds.end = ds.start + cbBuffer;

	ds.escStart = escBuffer;
	ds.escEnd = ds.escStart + (JSON_MAX_STACK_BUFFER_SIZE / sizeof(char));
	ds.escHeap = 0;
	ds.prv = dec->prv;
	ds.dec = dec;
	ds.dec->errorStr = NULL;
	ds.dec->errorOffset = NULL;
	ds.objDepth = 0;

	ds.dec = dec;

	ret = decode_any (&ds);

	if (ds.escHeap)
	{
		dec->free(ds.escStart);
	}

	if (!(dec->errorStr))
	{
		if ((ds.end - ds.start) > 0)
		{
			SkipWhitespace(&ds);
		}

		if (ds.start != ds.end && ret)
		{
			dec->releaseObject(ds.prv, ret);
			return SetError(&ds, -1, "Trailing data");
		}
	}

	return ret;
}
