#include "value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

JagValue *jag_val_null(void) {
    JagValue *v = (JagValue *)calloc(1, sizeof(JagValue));
    v->type = VAL_NULL;
    return v;
}

JagValue *jag_val_bool(bool b) {
    JagValue *v = (JagValue *)calloc(1, sizeof(JagValue));
    v->type = VAL_BOOL;
    v->boolean = b;
    return v;
}

JagValue *jag_val_num(int64_t n) {
    JagValue *v = (JagValue *)calloc(1, sizeof(JagValue));
    v->type = VAL_NUM;
    v->num = n;
    return v;
}

JagValue *jag_val_decimal(double d) {
    JagValue *v = (JagValue *)calloc(1, sizeof(JagValue));
    v->type = VAL_DECIMAL;
    v->decimal = d;
    return v;
}

JagValue *jag_val_scifi(double d) {
    JagValue *v = (JagValue *)calloc(1, sizeof(JagValue));
    v->type = VAL_SCIFI;
    v->decimal = d;
    return v;
}

JagValue *jag_val_string(const char *str) {
    JagValue *v = (JagValue *)calloc(1, sizeof(JagValue));
    v->type = VAL_STRING;
    v->string = str ? strdup(str) : strdup("");
    return v;
}

JagValue *jag_val_data(void) {
    JagValue *v = (JagValue *)calloc(1, sizeof(JagValue));
    v->type = VAL_DATA;
    v->map = (JagMap *)calloc(1, sizeof(JagMap));
    return v;
}

JagValue *jag_val_list(void) {
    JagValue *v = (JagValue *)calloc(1, sizeof(JagValue));
    v->type = VAL_LIST;
    v->list = (JagList *)calloc(1, sizeof(JagList));
    return v;
}

JagValue *jag_val_mixed_list(void) {
    JagValue *v = (JagValue *)calloc(1, sizeof(JagValue));
    v->type = VAL_MIXED_LIST;
    v->list = (JagList *)calloc(1, sizeof(JagList));
    return v;
}

void jag_val_free(JagValue *v) {
    if (!v) return;
    switch (v->type) {
        case VAL_STRING:
            if (v->string) free(v->string);
            break;
        case VAL_DATA:
            if (v->map) {
                for (size_t i = 0; i < v->map->count; i++) {
                    if (v->map->entries[i].key) free(v->map->entries[i].key);
                    if (v->map->entries[i].value) jag_val_free(v->map->entries[i].value);
                }
                if (v->map->entries) free(v->map->entries);
                free(v->map);
            }
            break;
        case VAL_LIST:
        case VAL_MIXED_LIST:
        case VAL_VECTOR:
            if (v->list) {
                for (size_t i = 0; i < v->list->count; i++) {
                    if (v->list->items[i]) jag_val_free(v->list->items[i]);
                }
                if (v->list->items) free(v->list->items);
                free(v->list);
            }
            break;
        default:
            break;
    }
    free(v);
}

JagValue *jag_val_dup(const JagValue *v) {
    if (!v) return NULL;
    switch (v->type) {
        case VAL_NULL: return jag_val_null();
        case VAL_BOOL: return jag_val_bool(v->boolean);
        case VAL_NUM: return jag_val_num(v->num);
        case VAL_DECIMAL: return jag_val_decimal(v->decimal);
        case VAL_SCIFI: return jag_val_scifi(v->decimal);
        case VAL_STRING: return jag_val_string(v->string);
        case VAL_DATA: {
            JagValue *m = jag_val_data();
            for (size_t i = 0; i < v->map->count; i++) {
                jag_map_set(m, v->map->entries[i].key, v->map->entries[i].value);
            }
            return m;
        }
        case VAL_LIST:
        case VAL_MIXED_LIST: {
            JagValue *l = v->type == VAL_LIST ? jag_val_list() : jag_val_mixed_list();
            for (size_t i = 0; i < v->list->count; i++) {
                jag_list_append(l, v->list->items[i]);
            }
            return l;
        }
        default:
            return jag_val_null();
    }
}

void jag_map_set(JagValue *map_val, const char *key, JagValue *val) {
    if (!map_val || map_val->type != VAL_DATA || !key) return;
    JagMap *m = map_val->map;

    for (size_t i = 0; i < m->count; i++) {
        if (strcmp(m->entries[i].key, key) == 0) {
            jag_val_free(m->entries[i].value);
            m->entries[i].value = jag_val_dup(val);
            return;
        }
    }

    if (m->count + 1 > m->capacity) {
        m->capacity = m->capacity == 0 ? 8 : m->capacity * 2;
        m->entries = (MapEntry *)realloc(m->entries, sizeof(MapEntry) * m->capacity);
    }

    m->entries[m->count].key = strdup(key);
    m->entries[m->count].value = jag_val_dup(val);
    m->count++;
}

JagValue *jag_map_get(JagValue *map_val, const char *key) {
    if (!map_val || map_val->type != VAL_DATA || !key) return NULL;
    JagMap *m = map_val->map;
    for (size_t i = 0; i < m->count; i++) {
        if (strcmp(m->entries[i].key, key) == 0) {
            return m->entries[i].value;
        }
    }
    return NULL;
}

void jag_list_append(JagValue *list_val, JagValue *item) {
    if (!list_val || (list_val->type != VAL_LIST && list_val->type != VAL_MIXED_LIST)) return;
    JagList *l = list_val->list;
    if (l->count + 1 > l->capacity) {
        l->capacity = l->capacity == 0 ? 8 : l->capacity * 2;
        l->items = (JagValue **)realloc(l->items, sizeof(JagValue *) * l->capacity);
    }
    l->items[l->count++] = jag_val_dup(item);
}

void jag_list_insert(JagValue *list_val, size_t index, JagValue *item) {
    if (!list_val || (list_val->type != VAL_LIST && list_val->type != VAL_MIXED_LIST)) return;
    JagList *l = list_val->list;
    if (index > l->count) index = l->count;
    if (l->count + 1 > l->capacity) {
        l->capacity = l->capacity == 0 ? 8 : l->capacity * 2;
        l->items = (JagValue **)realloc(l->items, sizeof(JagValue *) * l->capacity);
    }
    memmove(&l->items[index + 1], &l->items[index], sizeof(JagValue *) * (l->count - index));
    l->items[index] = jag_val_dup(item);
    l->count++;
}

void jag_list_delete(JagValue *list_val, size_t index) {
    if (!list_val || (list_val->type != VAL_LIST && list_val->type != VAL_MIXED_LIST)) return;
    JagList *l = list_val->list;
    if (index >= l->count) return;
    jag_val_free(l->items[index]);
    memmove(&l->items[index], &l->items[index + 1], sizeof(JagValue *) * (l->count - index - 1));
    l->count--;
}

JagValue *jag_list_get(JagValue *list_val, size_t index) {
    if (!list_val || (list_val->type != VAL_LIST && list_val->type != VAL_MIXED_LIST)) return NULL;
    JagList *l = list_val->list;
    if (index >= l->count) return NULL;
    return l->items[index];
}

char *jag_val_to_string(const JagValue *v) {
    if (!v) return strdup("null");
    char buf[512];
    switch (v->type) {
        case VAL_NULL: return strdup("null");
        case VAL_BOOL: return strdup(v->boolean ? "true" : "false");
        case VAL_NUM:
            snprintf(buf, sizeof(buf), "%ld", (long)v->num);
            return strdup(buf);
        case VAL_DECIMAL:
            snprintf(buf, sizeof(buf), "%.4f", v->decimal);
            return strdup(buf);
        case VAL_SCIFI:
            snprintf(buf, sizeof(buf), "%.2E", v->decimal);
            return strdup(buf);
        case VAL_STRING:
            return strdup(v->string ? v->string : "");
        case VAL_DATA: {
            char *out = strdup("{");
            for (size_t i = 0; i < v->map->count; i++) {
                char *val_str;
                if (v->map->entries[i].value && v->map->entries[i].value->type == VAL_STRING) {
                    snprintf(buf, sizeof(buf), "\"%s\"", v->map->entries[i].value->string);
                    val_str = strdup(buf);
                } else {
                    val_str = jag_val_to_string(v->map->entries[i].value);
                }
                char temp[512];
                snprintf(temp, sizeof(temp), "%s\"%s\":%s", (i > 0 ? ", " : " "), v->map->entries[i].key, val_str);
                free(val_str);
                char *nxt = (char *)realloc(out, strlen(out) + strlen(temp) + 2);
                strcat(nxt, temp);
                out = nxt;
            }
            strcat(out, " }");
            return out;
        }
        case VAL_LIST:
        case VAL_MIXED_LIST: {
            char *out = strdup("[");
            for (size_t i = 0; i < v->list->count; i++) {
                char *val_str;
                if (v->list->items[i] && v->list->items[i]->type == VAL_STRING) {
                    snprintf(buf, sizeof(buf), "\"%s\"", v->list->items[i]->string);
                    val_str = strdup(buf);
                } else {
                    val_str = jag_val_to_string(v->list->items[i]);
                }
                char temp[512];
                snprintf(temp, sizeof(temp), "%s%s", (i > 0 ? ", " : ""), val_str);
                free(val_str);
                char *nxt = (char *)realloc(out, strlen(out) + strlen(temp) + 2);
                strcat(nxt, temp);
                out = nxt;
            }
            strcat(out, "]");
            return out;
        }
        default:
            return strdup("<object>");
    }
}

void jag_live_on(JagValue *v) {
    char *str = jag_val_to_string(v);
    printf("%s\n", str);
    free(str);
}

void jag_live_deg(const char *expected_type, const char *label, JagValue *v) {
    char *str = jag_val_to_string(v);
    printf("[DEBUG %s] Expected: %s, Got: %s\n", label ? label : "", expected_type ? expected_type : "any", str);
    free(str);
}
