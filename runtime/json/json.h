#ifndef JAG_JSON_H
#define JAG_JSON_H

#include "runtime/core/value.h"

JagValue *jag_json_parse(const char *json_str);
char *jag_json_stringify(JagValue *val);

#endif
