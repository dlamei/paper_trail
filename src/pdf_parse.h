#pragma once

#include "utils.h"

struct String {
    u8 *data {NULL};
    usize size {0};
};

bool match_string(const String &s1, const char *s2);
void print_string(const String &c);

struct XRefTable {

};

struct PDF {
    String header {};
};

void load_file(const char *path, u8 **buffer, u64 *size);

PDF parse_pdf(u8 *content, u64 size);

