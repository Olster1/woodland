/*
A simple header include library like Sean Barret's stb libraries. 

This library makes it easy to handle utf8 encoded c strings (null terminated).

You have to #define EASY_STRING_IMPLEMENTATION before including the file to add the implementation part of it. 

////////////////////////////////////////////////////////////////////

How to use:

////////////////////////////////////////////////////////////////////

easyString_getSizeInBytes_utf8(char *str) - get the size of the string in bytes NOT INCLUDING THE NULL TERMINATOR 
easyString_getStringLength_utf8(char *str) - the number of glyphs in the string NOT INCLUDING THE NULL TERMINATOR. This function probably isn't very helpful. 

////////////////////////////////////////////////////////////////////
Probably the most useful function. See example below, but useful for rendering a utf8 string.  
easyUnicode_utf8_codepoint_To_Utf32_codepoint(char **streamPtr, int advancePtr) - get the next codepoint in the utf8 string, & you can choose to advance your string pointer.  

////////////////////////////////////////////////////////////////////

easyUnicode_utf8StreamToUtf32Stream_allocates(char *string) - turn the whole NULL TERMINATED string from utf8 to utf32 encoding
Use easyString_free_Utf32_string(ptr) to free the memory from the function above when finished

////////////////////////////////////////////////////////////////////
String compare functions:
int easyString_stringsMatch_withCount(char *a, int aLength, char *b, int bLength) - compares strings ignoring whether they're null terminated or not
int easyString_stringsMatch_null_and_count(char *a, char *b, int bLen) - compares a null terminated string to a string with length bLen
int easyString_stringsMatch_nullTerminated(char *a, char *b) - compares two null terminated strings 

////////////////////////////////////////////////////////////////////

Examples:

int main(int argc, char *args[]) {

	char *string = "გთხოვთ";
	int len = easyString_getStringLength_utf8(string);
	printf("%d\n", len);
	
	// Will return 6 glyphs 

	return 0;
 }

////////////////////////////////////////////////////////////////////

 int main(int argc, char *args[]) {

	char *str_utf8 = "გთხოვთ";
	char *at = string; //Take copy since str_utf8 is a read only variable
	
	while(*at) {
		unsigned int utf32Codepoint = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&at, 1);
	
		//Example api below if you wanted to render a utf8 string
		Glyph g = findGlyph(utf32Codepoint);
		renderGlyph(g);
		//
	}
	
	return 0;
 }

 ////////////////////////////////////////////////////////////////////

int main(int argc, char *args[]) {

	int doMatch = easyString_stringsMatch_withCount("გთხოვთ", 6, "გთხო", 4);
	//doMatch will return _false_

	int doMatch = easyString_stringsMatch_null_and_count("გთხო", "გთხოვთ", 4);
	//doMatch will return _true_

	int doMatch = easyString_stringsMatch_nullTerminated("გთხო", "გთხოვთ");
	//doMatch will return _false_
	
	return 0;
 }

*/
#ifndef EASY_STRING_UTF8_H
#define EASY_STRING_UTF8_H


#ifndef EASY_STRING_IMPLEMENTATION
#define EASY_STRING_IMPLEMENTATION 0
#endif

#ifndef EASY_HEADERS_ASSERT
#define EASY_HEADERS_ASSERT(statement) if(!(statement)) { int *i_ptr = 0; *(i_ptr) = 0; }
#endif

#ifndef EASY_HEADERS_ALLOC
#include <stdlib.h>
#define EASY_HEADERS_ALLOC(size) malloc(size)
#endif

#ifndef EASY_HEADERS_FREE
#include <stdlib.h>
#define EASY_HEADERS_FREE(ptr) free(ptr)
#endif

///////////////////////************ Header definitions start here *************////////////////////
int easyUnicode_isContinuationByte(unsigned char byte);
int easyUnicode_isSingleByte(unsigned char byte);
int easyUnicode_isLeadingByte(unsigned char byte);
	
//NOTE: this advances your pointer
unsigned int easyUnicode_utf8_codepoint_To_Utf32_codepoint(char **streamPtr, int advancePtr);

int easyString_getSizeInBytes_utf8(char *string);

int easyString_getStringLength_utf8(char *string);

unsigned int *easyUnicode_utf8StreamToUtf32Stream_allocates(char *stream);

void easyString_free_Utf32_string(char *string);

int easyString_stringsMatch_withCount(char *a, int aLength, char *b, int bLength);
int easyString_stringsMatch_null_and_count(char *a, char *b, int bLen);
int easyString_stringsMatch_nullTerminated(char *a, char *b);

int easyString_string_contains_utf8(char *string, char *sub_string);


///////////////////////*********** Implementation starts here **************////////////////////

#if EASY_STRING_IMPLEMENTATION

// The leading bytes and the continuation bytes do not share values 
// (continuation bytes start with 10 while single bytes start with 0 and longer lead bytes start with 11)

int easyUnicode_isContinuationByte(unsigned char byte) { //10
	unsigned char continuationMarker = (1 << 1);
	int result = (byte >> 6) == continuationMarker;

	return result;
}

int easyUnicode_isSingleByte(unsigned char byte) { //top bit 0
	unsigned char marker = 0;
	int result = (byte >> 7) == marker;

	return result;	
}

int easyUnicode_isLeadingByte(unsigned char byte) { //top bits 11
	unsigned char marker = (1 << 1 | 1 << 0);
	int result = (byte >> 6) == marker;

	return result;	
}


int easyUnicode_unicodeLength(unsigned char byte) {
	unsigned char bytes2 = (1 << 3 | 1 << 2);
	unsigned char bytes3 = (1 << 3 | 1 << 2 | 1 << 1);
	unsigned char bytes4 = (1 << 3 | 1 << 2 | 1 << 1 | 1 << 0);

	int result = 1;
	unsigned char shiftedByte = byte >> 4;
	if(!easyUnicode_isContinuationByte(byte) && !easyUnicode_isSingleByte(byte)) {
		EASY_HEADERS_ASSERT(easyUnicode_isLeadingByte(byte));
		if(shiftedByte == bytes2) { result = 2; }
		if(shiftedByte == bytes3) { result = 3; }
		if(shiftedByte == bytes4) { result = 4; }
		if(result == 1) EASY_HEADERS_ASSERT(!"invalid path");
	} 

	return result;

}

//NOTE: this advances your pointer
unsigned int easyUnicode_utf8_codepoint_To_Utf32_codepoint(char **streamPtr, int advancePtr) {
	unsigned char *stream = (unsigned char *)(*streamPtr);
	unsigned int result = 0;
	if(stream[0]) {
		
		unsigned int sixBitsFull = (1 << 5 | 1 << 4 | 1 << 3 | 1 << 2 | 1 << 1 | 1 << 0);
		unsigned int fiveBitsFull = (1 << 4 | 1 << 3 | 1 << 2 | 1 << 1 | 1 << 0);
		unsigned int fourBitsFull = (1 << 3 | 1 << 2 | 1 << 1 | 1 << 0);

		int unicodeLen = 1;
		if(easyUnicode_isContinuationByte(stream[0])) { 
			result =  '?';
		} else {
			unicodeLen = easyUnicode_unicodeLength(stream[0]);
		} 
		
		if(unicodeLen > 1) {
			EASY_HEADERS_ASSERT(easyUnicode_isLeadingByte(stream[0]));
			//needs to be decoded
			switch(unicodeLen) {
				case 2: {
					// printf("%s\n", "two byte unicode");
					unsigned int firstByte = stream[0];
					unsigned int secondByte = stream[1];
					EASY_HEADERS_ASSERT(easyUnicode_isContinuationByte(secondByte));
					result |= (secondByte & sixBitsFull);
					result |= ((firstByte & sixBitsFull) << 6);

					if(advancePtr) (*streamPtr) += 2;
				} break;
				case 3: {
					// printf("%s\n", "three byte unicode");
					unsigned int firstByte = stream[0];
					unsigned int secondByte = stream[1];
					unsigned int thirdByte = stream[2];
					EASY_HEADERS_ASSERT(easyUnicode_isContinuationByte(secondByte));
					EASY_HEADERS_ASSERT(easyUnicode_isContinuationByte(thirdByte));
					result |= (thirdByte & sixBitsFull);
					result |= ((secondByte & sixBitsFull) << 6);
					result |= ((firstByte & fiveBitsFull) << 12);

					if(advancePtr) (*streamPtr) += 3;
				} break;
				case 4: {
					// printf("%s\n", "four byte unicode");
					unsigned int firstByte = stream[0];
					unsigned int secondByte = stream[1];
					unsigned int thirdByte = stream[2];
					unsigned int fourthByte = stream[3];
					EASY_HEADERS_ASSERT(easyUnicode_isContinuationByte(secondByte));
					EASY_HEADERS_ASSERT(easyUnicode_isContinuationByte(thirdByte));
					EASY_HEADERS_ASSERT(easyUnicode_isContinuationByte(fourthByte));
					result |= (thirdByte & sixBitsFull);
					result |= ((secondByte & sixBitsFull) << 6);
					result |= ((firstByte & sixBitsFull) << 12);
					result |= ((firstByte & fourBitsFull) << 18);

					if(advancePtr) (*streamPtr) += 4;
				} break;
				default: {
					EASY_HEADERS_ASSERT(!"invalid path");
				}
			}
		} else {
			result = stream[0];
			if(advancePtr) { (*streamPtr) += 1; }
		}
	} else {

	}

	return result;
}

int size_of_last_utf8_codepoint_in_bytes(char *string, int byte_of_cursor) {
	int total_size_in_bytes = 0;

	bool found = false;

	while(byte_of_cursor > 0 && !found) {
		u8 byteAt = string[byte_of_cursor - 1];

		if(easyUnicode_isLeadingByte(byteAt) || easyUnicode_isSingleByte(byteAt)) {
			total_size_in_bytes++;
			found = true;
			break;
		} else if(easyUnicode_isContinuationByte(byteAt)) {
			byte_of_cursor--; //NOTE: Move back a byte
			total_size_in_bytes++;

			if(total_size_in_bytes > 3) {
				//NOTE: Should only ever be 3 continuation bytes in a row. 
				//NOTE: Could be binary file? 
				found = true;
				break;
			}
		} else {
			//NOTE: Isn't valid utf8 - could be binary file? 
			total_size_in_bytes = 1;
			found = true;
			break;
		}
	}

	return total_size_in_bytes;
}

int size_of_next_utf8_codepoint_in_bytes(char *string) {
	int total_size_in_bytes = 0;

	if(string[0]) {
		total_size_in_bytes = easyUnicode_unicodeLength(string[0]);
	}

	return total_size_in_bytes;
}


int easyString_getSizeInBytes_utf8(char *string) {
    unsigned int result = 0;
    unsigned char *at = (unsigned char *)string;
    while(*at) {
        result++;
        at++;
    }
    return result;
}

int easyString_getSizeInBytes_utf16(u16 *string) { //doesnt include null terminator
    size_t result = 0;
    u16 *at = string;
    while(*at) {
        result += 2;
        at++;
    }
    return result;
}


int easyString_getStringLength_utf8(char *string) {
    unsigned int result = 0;
    unsigned char *at = (unsigned char *)string;
    while(*at) {
        easyUnicode_utf8_codepoint_To_Utf32_codepoint((char **)&at, 1);
        result++;
    }
    return result;
}


//TODO: this would be a good place for simd. 
//NOTE: You have to free your string 

//IMPORTANT: string must be null terminated. 
unsigned int *easyUnicode_utf8StreamToUtf32Stream_allocates(char *stream) {
	unsigned int size = easyString_getStringLength_utf8(stream) + 1; //for null terminator
	unsigned int *result = (unsigned int *)(EASY_HEADERS_ALLOC(size*sizeof(unsigned int)));
	unsigned int *at = result;
	while(*stream) {
		char *a = stream;
		*at = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&stream, 1);
		EASY_HEADERS_ASSERT(stream != a);
		at++;
	}
	result[size - 1] = '\0';
	return result;
}

void easyString_free_Utf32_string(char *string) {
	EASY_HEADERS_FREE(string);
}


int easyString_stringsMatch_withCount(char *a, int aLength, char *b, int bLength) {
    int result = 1;
    
    int indexCount = 0;
    while(indexCount < aLength && indexCount < bLength) {
        indexCount++;
        result &= (*a == *b);
        a++;
        b++;
    }
    result &= (indexCount == bLength && indexCount == aLength);
    
    return result;
} 


int easyString_stringsMatch_null_and_count(char *a, char *b, int bLen) {
    int result = easyString_stringsMatch_withCount(a, easyString_getStringLength_utf8(a), b, bLen);
    return result;
}

int easyString_stringsMatch_nullTerminated(char *a, char *b) {
    int result = easyString_stringsMatch_withCount(a, easyString_getStringLength_utf8(a), b, easyString_getStringLength_utf8(b));
    return result;
}

//NOTE: Both strings must be null terminated
int easyString_string_contains_utf8(char *string, char *sub_string) {
	int result = false;

	char *at = string;

    while(at[0] && sub_string[0] && !result) {
		//NOTE: We got an initial match at the start, so see if the rest matches
		if(*at == sub_string[0]) {
			char *strMatch = at;
			char *sub_str_at = sub_string;

			bool temp_result = true;
			//NOTE: Start a string match
			while(*strMatch && *sub_str_at) {
				//NOTE: Try break the true value
				temp_result &= (strMatch[0] == sub_str_at[0]);
				strMatch++;
				sub_str_at++;
			}

			//NOTE: Check if the text string (not sub string) ran out first
			if(!strMatch[0] && sub_str_at[0]) {
				temp_result = false;
			} else if(temp_result) {
				//NOTE: We got a match
				result = true;
				break;
			}
		}

        at++;
    }

    return result;
}

struct String_Query_Search_Results {
	size_t byteOffsets[512];//NOTE Dynamic Array of byte offsets
	int byteOffsetCount; 
	int byteOffsetTotalCount;
	float sub_string_width; //NOTE: Without font scale
};

struct StringShiftTable {
	u32 runes[64];
	u32 shifts[64];	
	u32 shiftCount_inRunes;
};

static StringShiftTable string_utf8_build_shift_table(char *pattern_utf8) {
	StringShiftTable table = {};

	char *at = pattern_utf8;

	//NOTE: Get number of runes
	table.shiftCount_inRunes = easyString_getStringLength_utf8(pattern_utf8);

	int count = 0;

	while(*at) {
		assert(count < table.shiftCount_inRunes);
		u32 runeAt = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&at, true);

		table.shifts[count] = max(1, table.shiftCount_inRunes - count);
		table.runes[count] = runeAt;

		count++;
	}

		
	return table;
	
}

//NOTE: This assumes that string_at_end is equal to or longer than sub_string, well write into memroy if we're not
//		So I mark it as _ to make it 'hidden'
static bool string_utf8_matchStringBackwards_(char *string_at_end, char *sub_string, int sub_string_in_bytes) {
	bool result = false;
	if(sub_string_in_bytes > 0) {
		

		char *sub_end = sub_string + sub_string_in_bytes; 

		int byteAt = sub_string_in_bytes - 1;

		result = true;

		//NOTE: Move back off the end of the string
		string_at_end--;
		sub_end--;

		while(byteAt >= 0 && result) {
			result &= (*sub_end == *string_at_end);
			string_at_end--;
			sub_end--;
			byteAt--;	
		}
	}
	
	return result;
}

static int get_shift_from_table(StringShiftTable *table, u32 rune) {

	int result = 1;
	for(int i = 0; i < table->shiftCount_inRunes; ++i) {
		if(table->runes[i] == rune) {
			result = table->shifts[i];
			break;
		}
	}
	//NOTE: Default to 1 if couldn't find it

	return result;
}

//NOTE: Optimized by assuming sub_string will be smaller then text most of the time. 
static bool string_utf8_string_bigger_than_substring(char *text, char *sub_string) {
	bool result = true;
	while(*sub_string) {
		if(*text) {

		} else {
			//NOTE: text is smaller
			result = false;
			break;
		}
		text++;
		sub_string++;
	}

	return result;
}

//NOTE: boyer moore algorithm
static String_Query_Search_Results string_utf8_find_sub_string(char *text, char *sub_string_utf8) {
	String_Query_Search_Results result = {};
	if(string_utf8_string_bigger_than_substring(text, sub_string_utf8)) {
		
		StringShiftTable shift_table = string_utf8_build_shift_table(sub_string_utf8);

		char *at = text;

		int runeAt = shift_table.shiftCount_inRunes - 1;  

		int size_of_sub_string_in_bytes = easyString_getSizeInBytes_utf8(sub_string_utf8);

		//NOTE: Get the starting byteAt
		at += size_of_sub_string_in_bytes;

		bool looping = true;
		bool endNextLoop = false;
		while(looping) {

			if(string_utf8_matchStringBackwards_(at, sub_string_utf8, size_of_sub_string_in_bytes)) {
				//NOTE: Push into table as a found
				assert(512 > result.byteOffsetCount);
				int byte_at = (at - text);
				assert(byte_at >= 0);
				result.byteOffsets[result.byteOffsetCount++] = byte_at - size_of_sub_string_in_bytes; //NOTE: Move backwards to the start
			}
			
			u32 runeAt = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&at, false);

			int bytesToAdvance = get_shift_from_table(&shift_table, runeAt);

			char *temp = at + 1;

			//NOTE: This will move past the null terminator, so we need to check if we moved past it in this move
			at += bytesToAdvance;

			//NOTE: Check if we went past the null terminator
			//TODO: this could be slow? If we had the length of the full string we could use that instead. 
			while(temp < at) {
				if(!temp[0]) {
					//NOTE: Hit a null terminator 
					at = temp;
					break;
				} 
				temp++;
			}

			
			


			if(endNextLoop) {
				looping = false;
			}

			if(!at[0]) {
				endNextLoop = true;;
			}

		}
	}

	

	return result;
}

#endif // END OF IMPLEMENTATION
#endif // END OF HEADER INCLUDE


/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2021 Oliver Marsh
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
