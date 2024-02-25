#include "pdf_parse.h"

#include "utils.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <pthread.h>

#define ASSERT_NEXT_BYTE(parser) \
    GB_ASSERT_MSG(next_byte(parser), "next_byte called at eof")

// will advance until cond is met or eof is reached
#define ADVANCE_IF(parser, cond) \
    for (;;) if (!(cond) || !next_byte(parser)) break

#define ASSERT_IS_DIGIT(parser, ...) \
    GB_ASSERT_MSG(isdigit(parser->curr_byte), __VA_ARGS__)

#define PRINT_PARSE_FN() \
    printf("%s\n", BOOST_CURRENT_FUNCTION)

inline bool match_buffer(const Buffer &s1, const char *s2) {

    for (usize i = 0; i < s1.size; i++) {
        char c = s2[i];
        if (c == '\0' || c != s1.data[i])
            return false;
    }

    return s2[s1.size] == '\0';
}

inline void print_buffer(const Buffer &c) {
    printf("%.*s \n", (u32)c.size, c.data); 
}


struct Parser {
    u8 *content {NULL};
    u64 size {0};
    u64 curr_byte_indx {0};
    u8 curr_byte {0};

    PDF parsed_pdf{};
};

inline u64 cursor_position(Parser *p) {
    return p->curr_byte_indx;
}

inline bool reached_eof(Parser *p) {
    return p->curr_byte_indx == p->size - 1;
}

inline void update_curr_byte(Parser *p) {
    p->curr_byte = p->content[p->curr_byte_indx];
}

// false if eof was reached
inline bool next_byte(Parser *p) {
    if (p->curr_byte_indx + 1 < p->size) {
        p->curr_byte_indx += 1;
        update_curr_byte(p);
        return true;
    }

    return false;
}

// false if jumped over eof
inline bool fwd_n_bytes(Parser *p, u64 n) {
    if (p->curr_byte_indx + n < p->size) {
        p->curr_byte_indx += n;
        update_curr_byte(p);
        return true;
    }

    return false;
}

inline bool back_n_bytes(Parser *p,  u64 n) {
    if (p->curr_byte_indx - n < U64_MAX) {
        p->curr_byte_indx -= n;
        update_curr_byte(p);
        return true;
    }

    return false;
}

inline void goto_position(Parser *p, u64 indx) {
    if (indx < p->size) {
        p->curr_byte_indx = indx;
        update_curr_byte(p);
    } else {
        GB_PANIC("parser jump out of bounds");
    }
}

inline void skip_space(Parser *p) {
    ADVANCE_IF(p, isspace(p->curr_byte));
}


// compares the next n bytes, consumes them if they are equal
inline bool cmp_next_bytes(Parser *p, const char *bytes, u64 size) {
    u64 pos = cursor_position(p);

    for (u64 i = 0; i < size; i++) {
        if (pos + i >= p->size || p->content[pos + i] != bytes[i]) {
            goto_position(p, pos);
            return false;
        }
    }
    
    goto_position(p, pos);
    return true;
}

// compares the next n bytes, consumes them if they are equal
inline bool consume_bytes(Parser *p, const char *bytes, u64 size) {
    bool res = cmp_next_bytes(p, bytes, size);
    fwd_n_bytes(p, size);
    return res;
}

// compares the next byte, consumes it if they are equal
inline bool consume_byte(Parser *p, u8 byte) {
    if (p->curr_byte == byte) {
        next_byte(p);
        return true;
    }
    return false;
}

#define CMP_NEXT_BYTES(parser, lit) \
    cmp_next_bytes(parser, lit, sizeof(lit))

#define CMP_NEXT_BYTE(parser, byte) \
    parser->curr_byte == byte

#define CONSUME_BYTE(parser, byte) \
    GB_ASSERT_MSG(consume_byte(parser, byte), "expected %c, found: %c", (char)byte, (char)p->curr_byte)

#define CONSUME_BYTES(parser, bytes) \
    GB_ASSERT_MSG(consume_bytes(parser, bytes, sizeof(bytes)), "expected %s", bytes)


inline void print_next_n_bytes(Parser *p, u64 n) {
    u64 pos = cursor_position(p);
    for (u64 i = 0; i < n; i++) {
        printf("%c", (char)p->curr_byte);
        if (!next_byte(p)) break;
    }

    goto_position(p, pos);
}


Buffer parse_ansi_string(Parser *p) {
    PRINT_PARSE_FN();
    GB_ASSERT_MSG(isalpha(p->curr_byte), "expected ascii character");

    Buffer str{};
    str.data = p->content + p->curr_byte_indx;
    u64 start_indx = p->curr_byte_indx;

    ADVANCE_IF(p, (isalpha(p->curr_byte) || isdigit(p->curr_byte)));

    str.size = p->curr_byte_indx - start_indx;
    return str;
}

Buffer parse_comment(Parser *p) {
    PRINT_PARSE_FN();
    CONSUME_BYTE(p, '%');

    Buffer c{};
    
    c.data = p->content + p->curr_byte_indx;

    u64 start_indx = p->curr_byte_indx;

    ADVANCE_IF(p, (p->curr_byte != '\n'));


    skip_space(p);

    c.size = p->curr_byte_indx - start_indx;
    return c;
}

u64 parse_uint_len(Parser *p, u8 len) {
    PRINT_PARSE_FN();
    ASSERT_IS_DIGIT(p, "expected digit");

    u64 start_pos = cursor_position(p);

    fwd_n_bytes(p, len);

    u64 res {0};
    u32 acc {1};
    u32 end_pos = cursor_position(p);

    for (u32 i = end_pos - 1; i >= start_pos; i--) {
        u8 digit = p->content[i];
        GB_ASSERT_MSG(isdigit(digit), "expected digit");

        u8 v = p->content[i] - '0';
        res += v * acc;
        acc *= 10;
    }

    return res;
}

u64 parse_uint(Parser *p) {
    PRINT_PARSE_FN();
    ASSERT_IS_DIGIT(p, "expected digit");
    u64 start_pos = cursor_position(p);

    ADVANCE_IF(p, isdigit(p->curr_byte));

    u64 res {0};
    u32 acc {1};
    u32 end_pos = cursor_position(p);

    for (u32 i = end_pos - 1; i >= start_pos; i--) {
        u8 v = p->content[i] - '0';
        res += v * acc;
        acc *= 10;
    }

    return res;
}

inline void print_name(const Name &name) {
    print_buffer(name.buffer);
}

// e.g /Name
inline Name parse_name(Parser *p) {
    PRINT_PARSE_FN();
    CONSUME_BYTE(p, '/');
    Buffer str = parse_ansi_string(p);

    return Name {
        .buffer = str
    };
}

inline void print_string(const String &string) {
    print_buffer(string.buffer);
}

// e.g (The quick brown fox)
inline String parse_doc_string(Parser *p) {
    PRINT_PARSE_FN();
    CONSUME_BYTE(p, '(');
    Buffer str = parse_ansi_string(p);
    CONSUME_BYTE(p, ')');

    return String {
        .buffer = str
    };
}

i64 parse_integer(Parser *p) {
    PRINT_PARSE_FN();
    i8 sign = p->curr_byte == '-' ? -1 : 1;
    u64 uint = parse_uint(p);
    GB_ASSERT(uint < I64_MAX);
    return sign * (i64)uint;
}

void print_reference(const Reference &ref) {
    printf("%lu %lu %c", ref.obj_ref, ref.num, ref.c);
}

// e.g 2 0 R
Reference parse_reference(Parser *p) {
    PRINT_PARSE_FN();

    ASSERT_IS_DIGIT(p, "incorrect reference value");
    u64 obj_ref = parse_uint(p);
    next_byte(p);
    ASSERT_IS_DIGIT(p, "incorrect reference value");
    u64 num = parse_uint(p);

    skip_space(p);
    char c = p->curr_byte;
    ASSERT_NEXT_BYTE(p);

    println("c value: %c", c);


    return Reference {
        .obj_ref = obj_ref,
        .num = num,
        .c = c,
    };
}

Stream parse_stream(Parser *p) {
    CONSUME_BYTES(p, "stream");

    u64 start = cursor_position(p);
    ADVANCE_IF(p, CMP_NEXT_BYTES(p, "endstream"));
    u64 end = cursor_position(p);

    CONSUME_BYTES(p, "endstream");

    return Stream {
        .buffer = Buffer {
            .data = p->content + start,
            .size = end - start,
        }
    };
}

Object obj_from_integer(i64 integer) {
    return Object {
        .kind = OBJ_INTEGER,
        .data = Object::data_t {
            .integer = integer,
        },
    };
}

Object obj_from_name(Name name) {
    return Object {
        .kind = OBJ_NAME,
        .data = Object::data_t {
            .name = name,
        },
    };
}

Object obj_from_doc_string(String string) {
    return Object {
        .kind = OBJ_STRING,
        .data = Object::data_t {
            .string = string,
        },
    };
}

Object obj_from_dictionary(Dictionary dictionary) {
    return Object {
        .kind = OBJ_STRING,
        .data = Object::data_t {
            .dictionary = dictionary,
        },
    };
}

Object obj_from_array(Array array) {
    return Object {
        .kind = OBJ_ARRAY,
        .data = Object::data_t {
            .array = array,
        },
    };
}

Object obj_from_reference(Reference reference) {
    return Object {
        .kind = OBJ_REFERENCE,
        .data = Object::data_t {
            .reference = reference,
        },
    };
}

Object parse_object(Parser *p);

inline void print_array(const Array &array) {
    printf("[ ");
    for (u64 i = 0; i < array.count; i++) {
        print_object(array.data[i]);
        printf(" ");
    }
    printf("]");
}

// e.g [ 50 30 /Fred ]
Array parse_array(Parser *p) {
    PRINT_PARSE_FN();
    Array array {};

    CONSUME_BYTE(p, '[');

    for (;;) {
        skip_space(p);
        if (p->curr_byte == ']') break;

        Object object = parse_object(p);
        arrput(array.data, object);
        array.count += 1;
    }
    
    CONSUME_BYTE(p, ']');

    return array;
}

inline void print_dictionary(const Dictionary &dict) {
    printf("<< ");
    
    for (u64 i = 0; i < dict.count; i++) {
        print_name(dict.entries[i].name);
        print_object(dict.entries[i].object);
    }

    printf(">>");
}

// e.g << /Three 3 /Five 5 >>
Dictionary parse_dictionary(Parser *p) {
    print_next_n_bytes(p, 20);
    PRINT_PARSE_FN();
    Dictionary dict {};

    CONSUME_BYTE(p, '<');
    CONSUME_BYTE(p, '<');

    for (;;) {
        skip_space(p);
        if (p->curr_byte == '>') break;

        Name name = parse_name(p);
        Object object = parse_object(p);

        Dictionary::entry_t entry {
            .name = name,
            .object = object,
        };
        arrput(dict.entries, entry);
        dict.count += 1;
    }

    CONSUME_BYTE(p, '>');
    CONSUME_BYTE(p, '>');

    return dict;
}

inline void print_object(const Object &object) {
    if (object.kind == OBJ_INTEGER) {
        printf("%ld", object.data.integer);
    } else if (object.kind == OBJ_NAME) {
        print_name(object.data.name);
    } else if (object.kind == OBJ_STRING) {
        print_string(object.data.string);
    } else if (object.kind == OBJ_ARRAY) {
        print_array(object.data.array);
    } else if (object.kind == OBJ_DICTIONARY) {
        print_dictionary(object.data.dictionary);
    } else if (object.kind == OBJ_REFERENCE) {
        print_reference(object.data.reference);
    } else {
        GB_PANIC("unknown object kind: %u", object.kind);
    }
}

Object parse_object(Parser *p) {
    PRINT_PARSE_FN();
    Object obj {};

    skip_space(p);

    if (CMP_NEXT_BYTE(p, '/')) {
        Name name = parse_name(p);
        obj = obj_from_name(name);

    } else if (isdigit(p->curr_byte)) {
        u64 tmp_pos = cursor_position(p);
        i64 integer = parse_integer(p);
        skip_space(p);

        if (!isdigit(p->curr_byte)) {
            obj = obj_from_integer(integer);
        } else {
            goto_position(p, tmp_pos);
            Reference ref = parse_reference(p);
            obj = obj_from_reference(ref);
        }

    } else if (CMP_NEXT_BYTE(p, '(')) {
        String string = parse_doc_string(p);
        obj = obj_from_doc_string(string);

    } else if (CMP_NEXT_BYTES(p, "<<")) {
        Dictionary dict = parse_dictionary(p);
        obj = obj_from_dictionary(dict);

    } else if (CMP_NEXT_BYTE(p, '[')) {
        Array dict = parse_array(p);
        obj = obj_from_array(dict);

    } else {
        GB_PANIC("unknown object type! (starts with %c)", p->curr_byte);
    }

    return obj;
}

// entry 20 bytes long
// xxxxxxxxxx zzzzz z eol
// 1          2     3
//
// 1: 10-digit byte offset
// 2: 5-digit generation number
// 3: in-use 'n' or free 'f'
//

XRefTable::entry parse_xref_entry(Parser *p) {
    PRINT_PARSE_FN();
    GB_ASSERT_MSG(fwd_n_bytes(p, 20), "invalid xref entry: too short");

    // jump back to start
    back_n_bytes(p, 20);

    u64 byte_offset = parse_uint_len(p, 10);
    CONSUME_BYTE(p, ' ');
    u32 generation = (u32)parse_uint_len(p, 5);
    CONSUME_BYTE(p, ' ');
    bool in_use = (p->curr_byte == 'n');
    next_byte(p);
    skip_space(p);

    return XRefTable::entry {
        .byte_offset = byte_offset,
        .generation = generation,
        .in_use = in_use,
    };
}

void print_xref_entry(const XRefTable::entry &e) {
    printf("%010lu %05u %c", e.byte_offset, e.generation, e.in_use ? 'n' : 'f');
}

void print_xref_table(const XRefTable &t) {
    printf("xref\n");
    printf("%u %u\n", t.obj_id, t.obj_count);
    for (u64 i = 0; i < t.obj_count; i++) {
        print_xref_entry(t.entries[i]);
        println(" ");
    } 
}

XRefTable parse_xref_table(Parser *p) {
    PRINT_PARSE_FN();

    print_next_n_bytes(p, 20);

    CONSUME_BYTES(p, "xref");
    skip_space(p);
    u32 obj_id = (u32)parse_uint(p);
    skip_space(p);
    u32 obj_count = (u32)parse_uint(p);
    skip_space(p);

    XRefTable::entry *entries = (XRefTable::entry *)malloc(obj_count * sizeof(XRefTable::entry));

    for (u64 i = 0; i < obj_count; i++) {
        XRefTable::entry e = parse_xref_entry(p);
        *(entries + i) = e;
    }

    return XRefTable {
        .obj_id = obj_id,
            .obj_count = obj_count,
            .entries = entries,
    };

}

void delete_xref_table(XRefTable *t) {
    free(t->entries);
}


inline Parser make_parser(u8 *content, u64 size) {
    GB_ASSERT_MSG(size != 0, "empty pdf found");
    GB_ASSERT_MSG(content != NULL, "empty pdf found");

    PDF parsed_pdf {
        .byte_size = size,
    };

    return Parser {
        .content = content,
            .size = size,
            .curr_byte_indx = 0,
            .curr_byte = content[0],
            .parsed_pdf = parsed_pdf,
    };
}

// e.g
// %PDF-1.4
// .... (9 bytes)
void parse_header(Parser *p) {
    PRINT_PARSE_FN();
    Buffer header = parse_comment(p);
    GB_ASSERT_MSG(fwd_n_bytes(p, 9), "invalid pdf header");
    p->parsed_pdf.header = header;
}

#define COUNT_N_PARSED_BYTES(parse_fn) \
    u64 __start = p.curr_byte_indx; \
    parse_fn; \
    p.parsed_pdf.n_bytes_parsed += p.curr_byte_indx - __start;

PDF parse_pdf(u8 *content, u64 size) {
    PRINT_PARSE_FN();
    Parser p = make_parser(content, size);

    if (p.curr_byte == '%') {
        COUNT_N_PARSED_BYTES ( parse_header(&p);)
    }

    for (;;) {

        if (p.curr_byte == '%') {
            COUNT_N_PARSED_BYTES ( parse_comment(&p) );

        } else if (CMP_NEXT_BYTES(&p, "<<")) {
            COUNT_N_PARSED_BYTES( Dictionary dict = parse_dictionary(&p) );

        } else if (CMP_NEXT_BYTES(&p, "stream")) {
            COUNT_N_PARSED_BYTES( Stream stream = parse_stream(&p) );

        /* } else if (CMP_NEXT_BYTES(&p, "xref")) { */
        /*     COUNT_N_PARSED_BYTES( XRefTable table = parse_xref_table(&p); ); */
        /*     p.parsed_pdf.xref_table = table; */

        /* } else { */
        /*     if (!next_byte(&p)) { */
        /*         break; */
        /*     } */
        /* } */
        } else if (isalpha(p.curr_byte)) {
            Buffer str = parse_ansi_string(&p);

            if (match_buffer(str, "xref")) {
                COUNT_N_PARSED_BYTES ( parse_xref_table(&p);)
            }
        } else if (!next_byte(&p)) {
            break;
        }

    }

    return p.parsed_pdf;
}

void print_parsed_pdf(const PDF &pdf) {
    printf("byte size: %lu\n", pdf.byte_size);
    printf("%%-parsed: %f\n", (f64)pdf.n_bytes_parsed / (f64)pdf.byte_size);

    printf("%%");
    print_buffer(pdf.header);
    println(" ");

    print_xref_table(pdf.xref_table);
}

inline void delete_pdf(PDF *pdf) {
    delete_xref_table(&pdf->xref_table);
}


void load_file(const char *path, u8 **buffer, u64 *size) {
    u64 _size {0};
    u8 *_buffer {NULL};

    std::ifstream file(path);

    file.seekg(0, std::ios_base::end);
    _size = file.tellg();
    _buffer = (u8 *)malloc(_size);

    file.seekg(0);
    file.read((char *)_buffer, _size);

    GB_ASSERT_MSG(_buffer != NULL, "could not load file '%s'", path);

    *buffer = _buffer;
    *size = _size;
}
