#include "hash_table.h"

void htable_set(htable **ht, const unsigned char *key, size_t key_len,
                const unsigned char *value, size_t value_len) {
    
    htable* s;
    // first check if id exists via HASH_FIND
    HASH_FIND (hh, *ht, key, key_len, s);

    if (s == NULL) {
        s = (struct htable*)malloc(sizeof *s);

	s->key = (unsigned char*) malloc(key_len);
	s->value=(unsigned char*) malloc(value_len);
	
	s->key_len = key_len;
	s->value_len = value_len;
	
        memcpy(s->key, key, key_len);
        memcpy(s->value, value, value_len);
        
        HASH_ADD_KEYPTR(hh, *ht, s->key, s->key_len, s);
    }
    else {
    	free(s->value);
    	s->value=(unsigned char*) malloc(value_len);
    	s->value_len = value_len;
        memcpy(s->value, value, value_len);
        
    }
}

htable *htable_get(htable **ht, const unsigned char *key, size_t key_len) {
    /* TODO IMPLEMENT */
    struct htable *s;
    HASH_FIND(hh, *ht, key, key_len, s);
    return s;
}

int htable_delete(htable **ht, const unsigned char *key, size_t key_len) {
    /* TODO IMPLEMENT */
    struct htable *s;
    HASH_FIND(hh, *ht, key, key_len, s);

    if (s == NULL) {
        return -1;
    }
    free(s->key);
    free(s->value);
    HASH_DEL(*ht, s);
    //free(s);
    return 0;
}
