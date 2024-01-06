#pragma once

#include "utils.h"

struct String {
    u8 *data {NULL};
    usize size {0};
};

bool match_string(const String &s1, const char *s2);
void print_string(const String &c);

struct xref_entry {
    u64 byte_offset {0};
    u32 generation {0};
    bool in_use {false};
};

struct XRefTable {
    u32 obj_id;
    u32 obj_count;
    xref_entry *entries;
};

struct PDF {
    String header {};

    XRefTable xref_table;
};

PDF parse_pdf(u8 *content, u64 size);
void print_parsed_pdf(const PDF &);
void delete_pdf(PDF *);


void load_file(const char *path, u8 **buffer, u64 *size);


