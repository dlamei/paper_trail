#pragma once

#include "utils.h"

struct Object;

// mostly defines a slice inside the PDF buffer
struct Buffer {
    u8 *data {NULL};
    usize size {0};
};

bool match_buffer(const Buffer &s1, const char *s2);
void print_buffer(const Buffer &);

// e.g (The Quick Brown Fox)
struct String {
    Buffer buffer {0};
};


// e.g /Name
struct Name {
    Buffer buffer {0};
};

struct Reference {
    u64 obj_ref {0};
    u64 num {0};
    char c;
};

struct Stream {
    Buffer buffer {0};
};

// ordered collection of objects, e.g [50, 30, /Fred]
struct Array {
    Object *data {NULL};
    usize count {0};
};

// unordered map from name to object, e.g << /Three 3 /Five 5 >>
struct Dictionary {
    struct entry_t;
    entry_t *entries {NULL};
    usize count {0};
};

enum ObjectKind: u32 {
    OBJ_NAME,
    OBJ_INTEGER,
    OBJ_STRING,
    OBJ_REFERENCE,
    OBJ_DICTIONARY,
    OBJ_ARRAY,
};


// TODO: free
struct Object {
    union data_t {
        Name name;
        i64 integer;
        String string;
        Reference reference;

        Array array;
        Dictionary dictionary;
    };

    ObjectKind kind {0};
    data_t data {0};
};

struct Dictionary::entry_t {
    Name name {0};
    Object object {};
};

//TODO: reference, e.g 2 0 R, a reference to object 2


struct XRefTable {

    struct entry {
        u64 byte_offset {0};
        u32 generation {0};
        bool in_use {false};
    };

    u32 obj_id {0};
    u32 obj_count {0};
    entry *entries {NULL};
};

struct PDF {
    u64 byte_size {0};
    u64 n_bytes_parsed {0};

    Buffer header {};
    XRefTable xref_table {0};
};

PDF parse_pdf(u8 *content, u64 size);
void print_parsed_pdf(const PDF &);
void delete_pdf(PDF *);

void load_file(const char *path, u8 **buffer, u64 *size);

void print_array(const Array &);
void print_name(const Name &);
void print_string(const String &);
void print_reference(const Reference &);
void print_dictionary(const Dictionary &);
void print_object(const Object &);


