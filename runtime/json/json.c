#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void skip_json_ws(const char **s) {
    while (**s == ' ' || **s == '\t' || **s == '\r' || **s == '\n') (*s)++;
}

static JagValue *parse_json_value(const char **s);

static char *parse_json_string_raw(const char **s) {
    if (**s != '"') return NULL;
    (*s)++;
    const char *start = *s;
    while (**s && **s != '"') {
        if (**s == '\\' && *(*s + 1)) (*s)++;
        (*s)++;
    }
    size_t len = *s - start;
    char *res = strndup(start, len);
    if (**s == '"') (*s)++;
    return res;
}

static JagValue *parse_json_object(const char **s) {
    (*s)++;
    JagValue *m = jag_val_data();
    skip_json_ws(s);
    if (**s == '}') { (*s)++; return m; }

    for (;;) {
        skip_json_ws(s);
        if (**s != '"') break;
        char *key = parse_json_string_raw(s);
        skip_json_ws(s);
        if (**s == ':') (*s)++;
        skip_json_ws(s);
        JagValue *val = parse_json_value(s);
        jag_map_set(m, key, val);
        jag_val_free(val);
        free(key);

        skip_json_ws(s);
        if (**s == ',') { (*s)++; continue; }
        if (**s == '}') { (*s)++; break; }
    }
    return m;
}

static JagValue *parse_json_array(const char **s) {
    (*s)++;
    JagValue *l = jag_val_list();
    skip_json_ws(s);
    if (**s == ']') { (*s)++; return l; }

    for (;;) {
        skip_json_ws(s);
        JagValue *val = parse_json_value(s);
        jag_list_append(l, val);
        jag_val_free(val);

        skip_json_ws(s);
        if (**s == ',') { (*s)++; continue; }
        if (**s == ']') { (*s)++; break; }
    }
    return l;
}

static JagValue *parse_json_value(const char **s) {
    skip_json_ws(s);
    if (**s == '{') return parse_json_object(s);
    if (**s == '[') return parse_json_array(s);
    if (**s == '"') {
        char *str = parse_json_string_raw(s);
        JagValue *v = jag_val_string(str);
        free(str);
        return v;
    }
    if (isdigit(**s) || **s == '-') {
        const char *start = *s;
        bool is_dec = false;
        if (**s == '-') (*s)++;
        while (isdigit(**s)) (*s)++;
        if (**s == '.') {
            is_dec = true;
            (*s)++;
            while (isdigit(**s)) (*s)++;
        }
        if (is_dec) {
            return jag_val_decimal(atof(start));
        } else {
            return jag_val_num(atoll(start));
        }
    }
    if (strncmp(*s, "true", 4) == 0) { *s += 4; return jag_val_bool(true); }
    if (strncmp(*s, "false", 5) == 0) { *s += 5; return jag_val_bool(false); }
    if (strncmp(*s, "null", 4) == 0) { *s += 4; return jag_val_null(); }

    return jag_val_null();
}

JagValue *jag_json_parse(const char *json_str) {
    if (!json_str) return jag_val_null();
    const char *s = json_str;
    return parse_json_value(&s);
}

char *jag_json_stringify(JagValue *val) {
    return jag_val_to_string(val);
}
