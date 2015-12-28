#include "blob.h"
#include "blob_field.h"


static const int blob_type_minlen[BLOB_FIELD_LAST] = {
	[BLOB_FIELD_STRING] = 1,
	[BLOB_FIELD_INT8] = sizeof(uint8_t),
	[BLOB_FIELD_INT16] = sizeof(uint16_t),
	[BLOB_FIELD_INT32] = sizeof(uint32_t),
	[BLOB_FIELD_INT64] = sizeof(uint64_t),
};

/*const char *blob_field_name(struct blob_field *attr){
	if(!blob_field_has_name(attr)) return ""; 
	struct blob_name_hdr *hdr = (struct blob_name_hdr*)attr->data; 
	return (char*)hdr->name; 
}*/

void blob_field_fill_pad(struct blob_field *attr) {
	char *buf = (char *) attr;
	int len = blob_field_raw_pad_len(attr);
	int delta = len - blob_field_raw_len(attr);

	if (delta > 0)
		memset(buf + len - delta, 0, delta);
}

void blob_field_set_raw_len(struct blob_field *attr, unsigned int len){
	if(len < sizeof(struct blob_field)) len = sizeof(struct blob_field); 	
	len &= BLOB_FIELD_LEN_MASK;
	attr->id_len &= ~cpu_to_be32(BLOB_FIELD_LEN_MASK);
	attr->id_len |= cpu_to_be32(len);
}


bool blob_field_check_type(const void *ptr, unsigned int len, int type){
	const char *data = ptr;

	if (type >= BLOB_FIELD_LAST)
		return false;

	if (type >= BLOB_FIELD_INT8 && type <= BLOB_FIELD_INT64) {
		if (len != blob_type_minlen[type])
			return false;
	} else {
		if (len < blob_type_minlen[type])
			return false;
	}

	if (type == BLOB_FIELD_STRING && data[len - 1] != 0)
		return false;

	return true;
}

bool
blob_field_equal(const struct blob_field *a1, const struct blob_field *a2)
{
	if (!a1 && !a2)
		return true;

	if (!a1 || !a2)
		return false;

	if (blob_field_raw_pad_len(a1) != blob_field_raw_pad_len(a2))
		return false;

	return !memcmp(a1, a2, blob_field_raw_pad_len(a1));
}

//! returns the data of the attribute
void *blob_field_data(const struct blob_field *attr){
	if(!attr) return NULL; 
	return (void *) attr->data;
}


struct blob_field *blob_field_copy(struct blob_field *attr){
	struct blob_field *ret;
	int size = blob_field_raw_pad_len(attr);

	ret = malloc(size);
	if (!ret)
		return NULL;

	memcpy(ret, attr, size);
	return ret;
}

uint8_t 
blob_field_get_u8(const struct blob_field *attr){
	if(!attr) return 0; 
	return *((uint8_t *) attr->data);
}

void blob_field_set_u8(const struct blob_field *attr, uint8_t val){
	if(!attr) return; 
	*((uint8_t *) attr->data) = val;
}

uint16_t
blob_field_get_u16(const struct blob_field *attr){
	if(!attr) return 0; 
	uint16_t *tmp = (uint16_t*)attr->data;
	return be16_to_cpu(*tmp);
}

void
blob_field_set_u16(const struct blob_field *attr, uint16_t val){
	if(!attr) return; 
	uint16_t *tmp = (uint16_t*)attr->data;
	*tmp = cpu_to_be16(val); 
}

uint32_t
blob_field_get_u32(const struct blob_field *attr){
	if(!attr) return 0; 
	uint32_t *tmp = (uint32_t*)attr->data;
	return be32_to_cpu(*tmp);
}

void
blob_field_set_u32(const struct blob_field *attr, uint32_t val){
	if(!attr) return; 
	uint32_t *tmp = (uint32_t*)attr->data;
	*tmp = cpu_to_be32(val); 
}

uint64_t
blob_field_get_u64(const struct blob_field *attr){
	if(!attr) return 0; 
	uint32_t *ptr = (uint32_t *) blob_field_data(attr);
	uint64_t tmp = ((uint64_t) be32_to_cpu(ptr[0])) << 32;
	tmp |= be32_to_cpu(ptr[1]);
	return tmp;
}

int8_t
blob_field_get_i8(const struct blob_field *attr){
	if(!attr) return 0; 
	return blob_field_get_u8(attr);
}

void
blob_field_set_i8(const struct blob_field *attr, int8_t val){
	if(!attr) return; 
	blob_field_set_u8(attr, val);
}

int16_t
blob_field_get_i16(const struct blob_field *attr){
	if(!attr) return 0; 
	return blob_field_get_u16(attr);
}

void
blob_field_set_i16(const struct blob_field *attr, int16_t val){
	if(!attr) return; 
	blob_field_set_u16(attr, val);
}

int32_t
blob_field_get_i32(const struct blob_field *attr){
	if(!attr) return 0; 
	return blob_field_get_u32(attr);
}

void
blob_field_set_i32(const struct blob_field *attr, int32_t val){
	if(!attr) return; 
	blob_field_set_u32(attr, val);
}

int64_t
blob_field_get_i64(const struct blob_field *attr){
	if(!attr) return 0; 
	return blob_field_get_u64(attr);
}

float blob_field_get_f32(const struct blob_field *attr){
	if(!attr) return 0; 
	return unpack754_32(blob_field_get_u32(attr)); 
}

double blob_field_get_f64(const struct blob_field *attr){
	if(!attr) return 0; 
	return unpack754_64(blob_field_get_u64(attr)); 
}


long long blob_field_get_int(const struct blob_field *self){
	int type = blob_field_type(self); 
	switch(type){
		case BLOB_FIELD_INT8: return blob_field_get_i8(self); 
		case BLOB_FIELD_INT16: return blob_field_get_i16(self); 
		case BLOB_FIELD_INT32: return blob_field_get_i32(self); 
		case BLOB_FIELD_FLOAT32: return blob_field_get_f32(self); 
		case BLOB_FIELD_FLOAT64: return blob_field_get_f64(self); 
		case BLOB_FIELD_STRING: {
			long long val; 
			sscanf(self->data, "%lli", &val); 
			return val; 
		} 
	}
	return 0; 
}

double blob_field_get_float(const struct blob_field *self){
	int type = blob_field_type(self); 
	switch(type){
		case BLOB_FIELD_INT8: return blob_field_get_i8(self); 
		case BLOB_FIELD_INT16: return blob_field_get_i16(self); 
		case BLOB_FIELD_INT32: return blob_field_get_i32(self); 
		case BLOB_FIELD_FLOAT32: return blob_field_get_f32(self); 
		case BLOB_FIELD_FLOAT64: return blob_field_get_f64(self); 
		case BLOB_FIELD_STRING: {
			double val; 
			sscanf(self->data, "%lf", &val); 
			return val; 
		} 
	}
	return 0; 
}

bool blob_field_get_bool(const struct blob_field *self){
	return !!blob_field_get_int(self); 
}

const char *
blob_field_get_string(const struct blob_field *attr){
	if(!attr) return "(nil)"; 
	return attr->data;
}

void 
blob_field_get_raw(const struct blob_field *attr, uint8_t *data, size_t data_size){
	if(!attr) return; 
	memcpy(data, attr->data, data_size); 
}

//! returns the type of the attribute 
unsigned int blob_field_type(const struct blob_field *attr){
	if(!attr) return BLOB_FIELD_INVALID; 
	int id = (be32_to_cpu(attr->id_len) & BLOB_FIELD_ID_MASK) >> BLOB_FIELD_ID_SHIFT;
	return id;
}

void blob_field_set_type(struct blob_field *self, int type){
	int id_len = be32_to_cpu(self->id_len); 
	id_len = (id_len & ~BLOB_FIELD_ID_MASK) | (type << BLOB_FIELD_ID_SHIFT); 	
	self->id_len = cpu_to_be32(id_len);
}

//! returns full length of attribute
unsigned int
blob_field_raw_len(const struct blob_field *attr){
	if(!attr) return 0; 
	return (be32_to_cpu(attr->id_len) & BLOB_FIELD_LEN_MASK); 
}

//! includes length of data of the attribute
unsigned int
blob_field_data_len(const struct blob_field *attr){
	if(!attr) return 0; 
	return blob_field_raw_len(attr) - sizeof(struct blob_field);
}

//! returns padded length of full attribute
uint32_t 
blob_field_raw_pad_len(const struct blob_field *attr){
	if(!attr) return 0; 
	uint32_t len = blob_field_raw_len(attr);
	len = (len + BLOB_FIELD_ALIGN - 1) & ~(BLOB_FIELD_ALIGN - 1);
	return len;
}

struct blob_field *blob_field_first_child(const struct blob_field *self){
	if(!self) return NULL; 
	if(blob_field_raw_len(self) <= sizeof(struct blob_field)) return NULL; 
	return (struct blob_field*)blob_field_data(self); 
}

struct blob_field *blob_field_next_child(const struct blob_field *self, const struct blob_field *child){
	if(!child) return NULL;
	struct blob_field *ret = (struct blob_field *) ((char *) child + blob_field_raw_pad_len(child));
	// check if we are still within bounds
	size_t offset = (char*)ret - (char*)self; 
	if(offset >= blob_field_raw_pad_len(self)) return NULL; 
	return ret; 
}
bool _blob_field_validate(struct blob_field *attr, const char *signature, const char **nk){
	const char *k = signature; 
	//printf("validating %s\n", signature); 
	struct blob_field *field = blob_field_first_child(attr); 
	if(!field) return false; // correctly handle empty message!
	while(*k && field){
		//printf("KEY: %c\n", *k); 
		switch(*k){
			case '{': 
				if(blob_field_type(field) != BLOB_FIELD_TABLE) return false; 
				if(!_blob_field_validate(field, k + 1, &k)) return false; 
				//printf("continuing at %s\n", k); 
				break; 
			case '}': 
				//printf("closing\n"); 
				if(!field) {
					printf("exiting level\n"); 
					if(nk) *nk = k ; 
					return true; 
				}
				k = signature; 
				continue; 
				break; 
			case '[': 
				//printf("parsing array!\n"); 
				if(blob_field_type(field) != BLOB_FIELD_ARRAY) return false;  
				if(!_blob_field_validate(field, k + 1, &k)) return false; 
				break;
			case ']': 
				//printf("closing array\n"); 
				if(!field) return true; 
				k = signature; 
				continue; 
				break; 
			case 'i': 
				if(blob_field_type(field) != BLOB_FIELD_INT32 && blob_field_type(field) != BLOB_FIELD_INT8) return false; 
				break; 
			case 's': 
				if(blob_field_type(field) != BLOB_FIELD_STRING) return false; 
				break; 
			case 't': 
				if(blob_field_type(field) != BLOB_FIELD_TABLE) return false; 
				break; 
			case 'a': 
				if(blob_field_type(field) != BLOB_FIELD_ARRAY) return false; 
				break; 
			case 'v': 
				break; 
		}
		k++; 
		field = blob_field_next_child(attr, field); 
		//printf("next field %s %d\n", k, blob_field_type(field)); 
	}
	//printf("exiting at %s\n", k); 
	if(nk) *nk = k; 
	return true; 
}

bool blob_field_validate(struct blob_field *attr, const char *signature){
	return _blob_field_validate(attr, signature, NULL); 
}

bool blob_field_parse(struct blob_field *attr, const char *signature, struct blob_field **out, int out_size){
	memset(out, 0, sizeof(struct blob_field*) * out_size); 
	if(!blob_field_validate(attr, signature)) return false; 
	for(struct blob_field *a = blob_field_first_child(attr); a && out_size; a = blob_field_next_child(attr, a)){
		*out = a; 
		out++; 
		out_size--; 
	}
	return true; 
}
