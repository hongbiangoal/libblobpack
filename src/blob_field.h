/*
	copyright (c) 2010 felix fietkau <nbd@openwrt.org>
 	copyright (c) 2015 martin schröder <mkschreder.uk@gmail.com>

    this program is free software: you can redistribute it and/or modify
    it under the terms of the gnu general public license as published by
    the free software foundation, either version 3 of the license, or
    (at your option) any later version.

    this program is distributed in the hope that it will be useful,
    but without any warranty; without even the implied warranty of
    merchantability or fitness for a particular purpose.  see the
    gnu general public license for more details.

    you should have received a copy of the gnu general public license
    along with this program.  if not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <stdint.h>
#include "ieee754.h"

#define BLOB_FIELD_ID_MASK  0x7f000000
#define BLOB_FIELD_ID_SHIFT 24
#define BLOB_FIELD_LEN_MASK 0x00ffffff
#define BLOB_FIELD_ALIGN    4
#define BLOB_FIELD_HAS_NAME 0x80000000


#ifndef BLOB_FIELD_DEF_TAG
#define BLOB_FIELD_DEF_TAG
// a blob attribute header 
struct blob_field {
	uint32_t id_len;
	char data[];
} __attribute__ ((__packed__));
#endif

struct blob_field_policy {
	const char *name;
	uint8_t type;
};

//! returns the data of the attribute
const void *blob_field_data(const struct blob_field *attr); 

//! returns the type of the attribute 
uint8_t blob_field_type(const struct blob_field *attr); 
void blob_field_set_type(struct blob_field *self, int type); 

//! returns full length of attribute
unsigned int blob_field_raw_len(const struct blob_field *attr); 
void blob_field_set_raw_len(struct blob_field *attr, uint32_t raw_len); 

//! includes length of data of the attribute
unsigned int blob_field_data_len(const struct blob_field *attr); 

//! returns padded length of full attribute
uint32_t blob_field_raw_pad_len(const struct blob_field *attr); 

//! get current attribute as an integer

bool blob_field_get_bool(const struct blob_field *self); 
long long int blob_field_get_int(const struct blob_field *self); 
double blob_field_get_real(const struct blob_field *self); 
const char *blob_field_get_string(const struct blob_field *self); 
//size_t blob_field_get_binary(const struct blob_field *attr, uint8_t *data, size_t data_size); 
/*
uint8_t blob_field_get_u8(const struct blob_field *attr); 
uint8_t blob_field_set_u8(const struct blob_field *attr, uint8_t val); 
uint16_t blob_field_get_u16(const struct blob_field *attr); 
void blob_field_set_u16(const struct blob_field *attr, uint16_t val); 
uint32_t blob_field_get_u32(const struct blob_field *attr); 
void blob_field_set_u32(const struct blob_field *attr, uint32_t val); 
uint64_t blob_field_get_u64(const struct blob_field *attr); 
int8_t blob_field_get_i8(const struct blob_field *attr); 
void blob_field_set_i8(const struct blob_field *attr, int8_t val); 
int16_t blob_field_get_i16(const struct blob_field *attr); 
void blob_field_set_i16(const struct blob_field *attr, int16_t val); 
int32_t blob_field_get_i32(const struct blob_field *attr); 
void blob_field_set_i32(const struct blob_field *attr, int32_t val); 
int64_t blob_field_get_i64(const struct blob_field *attr); 
const char *blob_field_get_string(const struct blob_field *attr); 
void blob_field_get_raw(const struct blob_field *attr, uint8_t *data, size_t data_size); 
float blob_field_get_float(const struct blob_field *attr); 
double blob_field_get_double(const struct blob_field *attr); 
*/

void blob_field_fill_pad(struct blob_field *attr);
//extern void blob_field_set_raw_len(struct blob_field *attr, unsigned int len);
bool blob_field_equal(const struct blob_field *a1, const struct blob_field *a2);
struct blob_field *blob_field_copy(struct blob_field *attr);
//bool blob_field_check_type(const void *ptr, unsigned int len, int type);

const struct blob_field *blob_field_first_child(const struct blob_field *self); 
const struct blob_field *blob_field_next_child(const struct blob_field *self, const struct blob_field *child); 

void blob_field_dump(const struct blob_field *self); 

bool blob_field_validate(const struct blob_field *attr, const char *signature); 

bool blob_field_parse(const struct blob_field *attr, const char *signature, const struct blob_field **out, int out_size); 

struct blob_policy; 
bool blob_field_parse_values(const struct blob_field *attr, struct blob_policy *policy, int policy_size); 

#define blob_field_for_each_kv(attr, key, value) \
	for(key = blob_field_first_child(attr), value = blob_field_next_child(attr, key); \
		key && value; \
		key = blob_field_next_child(attr, value), value = blob_field_next_child(attr, key))

#define blob_field_for_each_child(attr, child) \
	for(child = blob_field_first_child(attr); child; child = blob_field_next_child(attr, child))
