#include "pdf_parse.h"

#include "utils.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <pthread.h>

// will advance until cond is met or eof is reached
#define ADVANCE_IF(parser, cond) \
    for (;;) if (!cond || !next_byte(parser)) break

struct Parser {
    u8 *content {NULL};
    u64 size {0};
    u64 curr_byte_indx {0};
    u8 curr_byte {0};

    bool in_comment {false};

    PDF parsed_pdf{};
};

inline bool reached_eof(Parser *p) {
    return p->curr_byte_indx == p->size;
}

inline void update_curr_byte(Parser *p) {
    p->curr_byte = p->content[p->curr_byte_indx];
}

inline bool next_byte(Parser *p) {
    if (p->curr_byte_indx + 1 < p->size) {
        p->curr_byte_indx += 1;
        update_curr_byte(p);
        return true;
    }

    return false;
}

inline bool next_n_bytes(Parser *p, u64 n) {
    if (p->curr_byte_indx + n < p->size) {
        p->curr_byte_indx += n;
        update_curr_byte(p);
        return true;
    }

    return false;
}

inline bool prev_n_bytes(Parser *p,  u64 n) {
    if (p->curr_byte_indx - n < U64_MAX) {
        p->curr_byte_indx -= n;
        update_curr_byte(p);
        return true;
    }

    return false;
}

inline void print_next_n_bytes(Parser *p, u64 n) {
    for (u64 i = 0; i < n; i++) {
        printf("%c", (char)p->curr_byte);
        if (!next_byte(p)) break;
    }
}

#define expect_byte(parser, byte) \
    GB_ASSERT_MSG(p->curr_byte == byte, "expected %c, found: %c", (char)byte, (char)p->curr_byte)

/*
inline void expect_byte(Parser *p, u8 byte) {
    GB_ASSERT_MSG(p->curr_byte == byte, "expected %c, found: %c", (char)byte, (char)p->curr_byte);
}
*/


String parse_comment(Parser *p) {
    expect_byte(p, '%');

    String c{};
    
    if (!next_byte(p)) {
        return c;
    }

    c.data = p->content + p->curr_byte_indx;

    u64 start_indx = p->curr_byte_indx;

    ADVANCE_IF(p, (p->curr_byte != '\n'));


    if (!reached_eof(p)) {
        expect_byte(p, '\n');
    }

    c.size = p->curr_byte_indx - start_indx;
    return c;
}

inline void skip_space(Parser *p) {
    ADVANCE_IF(p, isspace(p->curr_byte));
}

u64 parse_uint_len(Parser *p, u8 len) {
    GB_ASSERT_MSG(isdigit(p->curr_byte), "expected digit");
    u64 start_indx = p->curr_byte_indx;

    next_n_bytes(p, len);

    u64 res {0};
    u32 acc {1};
    u32 end_indx = p->curr_byte_indx;

    for (u32 i = end_indx - 1; i >= start_indx; i--) {
        u8 digit = p->content[i];
        GB_ASSERT_MSG(isdigit(digit), "expected digit");

        u8 v = p->content[i] - '0';
        res += v * acc;
        acc *= 10;
    }

    return res;
}

u64 parse_uint(Parser *p) {
    GB_ASSERT_MSG(isdigit(p->curr_byte), "expected digit");
    u64 start_indx = p->curr_byte_indx;

    ADVANCE_IF(p, isdigit(p->curr_byte));

    u64 res {0};
    u32 acc {1};
    u32 end_indx = p->curr_byte_indx;

    for (u32 i = end_indx - 1; i >= start_indx; i--) {
        u8 v = p->content[i] - '0';
        res += v * acc;
        acc *= 10;
    }

    return res;
}

String parse_ansi_string(Parser *p) {
    GB_ASSERT_MSG(isalpha(p->curr_byte), "expected ascii character");

    String str{};
    str.data = p->content + p->curr_byte_indx;
    u64 start_indx = p->curr_byte_indx;

    ADVANCE_IF(p, isalpha(p->curr_byte));

    str.size = p->curr_byte_indx - start_indx;
    return str;
}


// entry 20 bytes long
// xxxxxxxxxx zzzzz z eol
// 1          2     3
//
// 1: 10-digit byte offset
// 2: 5-digit generation number
// 3: in-use 'n' or free 'f'
//

xref_entry parse_xref_entry(Parser *p) {
    GB_ASSERT_MSG(next_n_bytes(p, 20), "invalid xref entry: too short");

    // jump back to start
    prev_n_bytes(p, 20);

    u64 byte_offset = parse_uint_len(p, 10);

    expect_byte(p, ' ');
    next_byte(p);

    u32 generation = (u32)parse_uint_len(p, 5);

    expect_byte(p, ' ');
    next_byte(p);

    bool in_use = (p->curr_byte == 'n');

    next_byte(p);
    expect_byte(p, ' ');

    next_byte(p);
    expect_byte(p, '\n');

    next_byte(p);

    return xref_entry {
        .byte_offset = byte_offset,
        .generation = generation,
        .in_use = in_use,
    };
}

void print_xref_entry(const xref_entry &e) {
    printf("%010lu %05u %c", e.byte_offset, e.generation, e.in_use ? 'n' : 'f');
}

void print_xref_table(const XRefTable &t) {
    printf("xref\n");
    printf("%u %u\n", t.obj_id, t.obj_count);
    for (u64 i = 0; i < t.obj_count; i++) {
        print_xref_entry(t.entries[i]);
        println();
    } 
}

void parse_xref_table(Parser *p) {
    skip_space(p);
    u32 obj_id = (u32)parse_uint(p);
    skip_space(p);
    u32 obj_count = (u32)parse_uint(p);
    skip_space(p);

    xref_entry *entries = (xref_entry *)malloc(obj_count * sizeof(xref_entry));

    for (u64 i = 0; i < obj_count; i++) {
        xref_entry e = parse_xref_entry(p);
        *(entries + i) = e;
    }

    XRefTable xref_table {
        .obj_id = obj_id,
            .obj_count = obj_count,
            .entries = entries,
    };

    p->parsed_pdf.xref_table = xref_table;
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

void parse_header(Parser *p) {
    String header = parse_comment(p);
    GB_ASSERT_MSG(next_n_bytes(p, 9), "invalid pdf header");
    p->parsed_pdf.header = header;
}

#define COUNT_N_PARSED_BYTES(parse_fn) \
    u64 __start = p.curr_byte_indx; \
    parse_fn \
    p.parsed_pdf.n_bytes_parsed += p.curr_byte_indx - __start;

PDF parse_pdf(u8 *content, u64 size) {
    Parser p = make_parser(content, size);

    if (p.curr_byte == '%') {
        COUNT_N_PARSED_BYTES (
            parse_header(&p);
        )
    }

    for (;;) {
        if (p.curr_byte == '%') {
            COUNT_N_PARSED_BYTES (
                parse_comment(&p);
            )

        } else if (isalpha(p.curr_byte)) {
            String str = parse_ansi_string(&p);

            if (match_string(str, "xref")) {
                COUNT_N_PARSED_BYTES (
                    parse_xref_table(&p);
                )
            }
        }

        if (!next_byte(&p))
            break;
    }

    return p.parsed_pdf;
}

void print_parsed_pdf(const PDF &pdf) {
    printf("byte size: %lu\n", pdf.byte_size);
    printf("%%-parsed: %f\n", (f64)pdf.n_bytes_parsed / (f64)pdf.byte_size);

    printf("%%");
    print_string(pdf.header);
    println();

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

inline bool match_string(const String &s1, const char *s2) {

    for (usize i = 0; i < s1.size; i++) {
        char c = s2[i];
        if (c == '\0' || c != s1.data[i])
            return false;
    }

    return s2[s1.size] == '\0';
}

inline void print_string(const String &c) {
    printf("%.*s \n", (u32)c.size, c.data); 
}

