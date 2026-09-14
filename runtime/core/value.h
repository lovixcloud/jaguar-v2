#ifndef JAG_VALUE_H
#define JAG_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    VAL_NULL,
    VAL_BOOL,
    VAL_NUM,
    VAL_DECIMAL,
    VAL_SCIFI,
    VAL_STRING,
    VAL_DATA,
    VAL_LIST,
    VAL_MIXED_LIST,
    VAL_VECTOR,
    VAL_MATRIX,
    VAL_TASK,
    VAL_WORKER,
    VAL_SOCKET,
    VAL_CLOSURE
} ValueType;

typedef struct JagValue JagValue;

typedef struct {
    char *key;
    JagValue *value;
} MapEntry;

typedef struct {
    MapEntry *entries;
    size_t count;
    size_t capacity;
} JagMap;

typedef struct {
    JagValue **items;
    size_t count;
    size_t capacity;
} JagList;

struct JagValue {
    ValueType type;
    union {
        bool boolean;
        int64_t num;
        double decimal;
        char *string;
        JagMap *map;
        JagList *list;
        void *ptr;
    };
};

JagValue *jag_val_null(void);
JagValue *jag_val_bool(bool b);
JagValue *jag_val_num(int64_t n);
JagValue *jag_val_decimal(double d);
JagValue *jag_val_scifi(double d);
JagValue *jag_val_string(const char *str);
JagValue *jag_val_data(void);
JagValue *jag_val_list(void);
JagValue *jag_val_mixed_list(void);

void jag_val_free(JagValue *v);
JagValue *jag_val_dup(const JagValue *v);

void jag_map_set(JagValue *map_val, const char *key, JagValue *val);
JagValue *jag_map_get(JagValue *map_val, const char *key);

void jag_list_append(JagValue *list_val, JagValue *item);
void jag_list_insert(JagValue *list_val, size_t index, JagValue *item);
void jag_list_delete(JagValue *list_val, size_t index);
JagValue *jag_list_get(JagValue *list_val, size_t index);

char *jag_val_to_string(const JagValue *v);
void jag_live_on(JagValue *v);
void jag_live_deg(const char *expected_type, const char *label, JagValue *v);

#endif
