#include "pdf_parse.h"

#include "decompress.h"
#include "utils.h"

#include <string.h>
#include <ctype.h>

#define ASSERT_NEXT_BYTE(parser) \
    GB_ASSERT_MSG(next_byte(parser), "next_byte called at eof")

// will advance until cond is met or eof is reached
#define ADVANCE_IF(parser, cond) \
    for (;;) if (!(cond) || !next_byte(parser)) break

#define ASSERT_IS_DIGIT(parser, ...) \
    GB_ASSERT_MSG(isdigit(parser->curr_byte), __VA_ARGS__)

#define PRINT_PARSE_FN() \
    /* printf("%s\n", BOOST_CURRENT_FUNCTION) */


typedef struct Parser {
    u8 *buffer;
    u64 size;
    u64 cursor;
    u8 curr_byte;
} Parser;

u64 cursor_pos(Parser *p) {
    return p->cursor;
}

u8 *cursor_ptr(Parser *p) {
    return p->buffer + p->cursor;
}

bool reached_eof(Parser *p) {
    return p->cursor == p->size - 1;
}

void update_curr_byte(Parser *p) {
    p->curr_byte = p->buffer[p->cursor];
}

// false if eof was reached
bool next_byte(Parser *p) {
    if (p->cursor + 1 < p->size) {
        p->cursor += 1;
        update_curr_byte(p);
        return true;
    }

    return false;
}

bool prev_byte(Parser *p) {
    if (p->cursor - 1 < U64_MAX) {
        p->cursor -= 1;
        update_curr_byte(p);
        return true;
    }

    return false;
}

// false if jumped over eof
bool fwd_n_bytes(Parser *p, u64 n) {
    if (p->cursor + n < p->size) {
        p->cursor += n;
        update_curr_byte(p);
        return true;
    }

    return false;
}

local inline bool back_n_bytes(Parser *p,  u64 n) {
    if (p->cursor - n < U64_MAX) {
        p->cursor -= n;
        update_curr_byte(p);
        return true;
    }

    return false;
}

local inline void goto_offset(Parser *p, u64 indx) {
    if (indx < p->size) {
        p->cursor = indx;
        update_curr_byte(p);
    } else {
        GB_PANIC("parser jump out of bounds");
    }
}

void goto_eof(Parser *p) {
    goto_offset(p, p->size - 1);
}


// compares the next n bytes, consumes them if they are equal
bool cmp_next_bytes(Parser *p, const char *bytes, u64 size) {
    u64 pos = cursor_pos(p);

    for (u64 i = 0; i < size; i++) {
        if (pos + i >= p->size || p->buffer[pos + i] != bytes[i]) {
            goto_offset(p, pos);
            return false;
        }
    }

    goto_offset(p, pos);
    return true;
}

// compares the next n bytes, consumes them if they are equal
local inline bool consume_bytes(Parser *p, const char *bytes, u64 size) {
    bool res = cmp_next_bytes(p, bytes, size);
    if (res) fwd_n_bytes(p, size);
    return res;
}

// compares the next byte, consumes it if they are equal
local inline bool consume_byte(Parser *p, u8 byte) {
    if (p->curr_byte == byte) {
        next_byte(p);
        return true;
    }
    return false;
}

#define CURR_BYTES(parser, lit) \
    cmp_next_bytes(parser, lit, sizeof(lit) - 1)

#define CURR_BYTE(parser, byte) \
    parser->curr_byte == byte

#define CONSUME_BYTES(parser, bytes) \
    consume_bytes(parser, bytes, sizeof(bytes) - 1)

#define EXPECT_BYTE(parser, byte) \
    GB_ASSERT_MSG(consume_byte(parser, byte), "expected %c, found: %c", (char)byte, (char)p->curr_byte)

#define EXPECT_BYTES(parser, bytes) \
    GB_ASSERT_MSG(consume_bytes(parser, bytes, sizeof(bytes) - 1), "could not find %s", bytes)

void skip_space(Parser *p) {
    ADVANCE_IF(p, isspace(p->curr_byte));
}

void skip_newline(Parser *p) {
    ADVANCE_IF(p, CURR_BYTE(p, '\n') || CURR_BYTE(p, '\r'));
}

void print_next_n_bytes(Parser *p, u64 n) {
    u64 pos = cursor_pos(p);
    for (u64 i = 0; i < n; i++) {
        printf("%c", (char)p->curr_byte);
        if (!next_byte(p)) break;
    }

    println(" ");
    goto_offset(p, pos);
}

void print_prev_n_bytes(Parser *p, u64 n) {
    u64 pos = cursor_pos(p);
    back_n_bytes(p, n);
    print_next_n_bytes(p, n);    
    goto_offset(p, pos);
}


PDFSlice parse_ansi_string(Parser *p) {
    PRINT_PARSE_FN();
    GB_ASSERT_MSG(isalpha(p->curr_byte), "expected ascii character");

    PDFSlice str = {0};
    str.ptr = p->buffer + p->cursor;
    u64 start_indx = p->cursor;

    ADVANCE_IF(p, (isalpha(p->curr_byte) || isdigit(p->curr_byte)));

    str.len = p->cursor - start_indx;
    return str;
}

PDFSlice parse_comment(Parser *p) {
    PRINT_PARSE_FN();
    EXPECT_BYTE(p, '%');

    PDFSlice c = {0};

    c.ptr = p->buffer + p->cursor;

    u64 start_indx = p->cursor;

    ADVANCE_IF(p, (p->curr_byte != '\n'));


    skip_space(p);

    c.len = p->cursor - start_indx;
    return c;
}

u64 parse_uint_len(Parser *p, u8 len) {
    PRINT_PARSE_FN();
    ASSERT_IS_DIGIT(p, "expected digit");

    u64 start_pos = cursor_pos(p);

    fwd_n_bytes(p, len);

    u64 res = 0;
    u32 acc = 1;
    u32 end_pos = cursor_pos(p);

    for (u32 i = end_pos - 1; i >= start_pos; i--) {
        u8 digit = p->buffer[i];
        GB_ASSERT_MSG(isdigit(digit), "expected digit");

        u8 v = p->buffer[i] - '0';
        res += v * acc;
        acc *= 10;
    }

    return res;
}

u64 parse_uint(Parser *p) {
    PRINT_PARSE_FN();
    ASSERT_IS_DIGIT(p, "expected digit");
    u64 start_pos = cursor_pos(p);

    ADVANCE_IF(p, isdigit(p->curr_byte));

    u64 res = 0;
    u32 acc = 1;
    u32 end_pos = cursor_pos(p);

    for (u32 i = end_pos - 1; i >= start_pos; i--) {
        u8 v = p->buffer[i] - '0';
        res += v * acc;
        acc *= 10;
    }

    return res;
}

// e.g /Name
Name parse_name(Parser *p) {
    PDFSlice slice = {0};

    PRINT_PARSE_FN();
    EXPECT_BYTE(p, '/');

    u64 start = cursor_pos(p);
    slice.ptr = p->buffer + start;

    ADVANCE_IF(p, !(CURR_BYTE(p, ' ') || CURR_BYTE(p, '/')
                || CURR_BYTE(p, '<') || CURR_BYTE(p, '>')
                || CURR_BYTE(p, '(') || CURR_BYTE(p, ')')
                || CURR_BYTE(p, '[') || CURR_BYTE(p, ']')
                || CURR_BYTE(p, '\n') || CURR_BYTE(p, '\r')
                ));
    slice.len = cursor_pos(p) - start;

    GB_ASSERT_MSG(slice.len != 0, "zero length name");

    return (Name) { .slice = slice };
}

// e.g <FEFF005700720069007400650072>
HexString parse_hex_string(Parser *p) {
    PRINT_PARSE_FN();

    u64 start = cursor_pos(p);
    PDFSlice slice = {0};
    slice.ptr = p->buffer + start;

    EXPECT_BYTE(p, '<');
    for (;;) {
        if (p->curr_byte == '>' || !next_byte(p))
            break;
    }
    EXPECT_BYTE(p, '>');

    slice.len = cursor_pos(p) - start;

    return (HexString) {
        .slice = slice,
    };
}

// e.g (The quick brown fox)
// TODO: escape character 
PDFString parse_string(Parser *p) {
    PRINT_PARSE_FN();

    PDFSlice slice = {0};
    slice.ptr = cursor_ptr(p);

    EXPECT_BYTE(p, '(');
    u32 level = 1;
    for (;;) {
        if (CURR_BYTE(p, '(')) {
            level += 1;
        } else if (CURR_BYTE(p, ')')) {
            level -= 1;
        }

        if (level == 0) {
            break;
        } else {
            next_byte(p);
        }
    }
    slice.len = cursor_ptr(p) - slice.ptr;
    EXPECT_BYTE(p, ')');


    return (PDFString) { .slice = slice };
}

Boolean parse_boolean(Parser *p) {
    bool value = false;
    if (CONSUME_BYTES(p, "true")) {
        value = true;
    } else if (CONSUME_BYTES(p, "false")) {
        value = false;
    } else {
        GB_PANIC("could not parse boolean");
    }

    return (Boolean) { .value = value };
}

Integer parse_integer(Parser *p) {
    PRINT_PARSE_FN();

    i8 sign = 1;
    if (consume_byte(p, '-')) {
        sign = -1;
    }
    consume_byte(p, '+');

    u64 uint = parse_uint(p);
    GB_ASSERT(uint < I64_MAX);
    return (Integer) { .value = sign * (i64)uint, };
}

bool is_object(Parser *p) {
    return false;
}

bool is_reference(Parser *p) {
    u64 pos = cursor_pos(p);
    bool res = false;

    if (!isdigit(p->curr_byte)) goto reset;
    ADVANCE_IF(p, isdigit(p->curr_byte));

    if (!consume_byte(p, ' ')) goto reset;
    skip_space(p);

    if (!isdigit(p->curr_byte)) goto reset;
    ADVANCE_IF(p, isdigit(p->curr_byte));

    if (!consume_byte(p, ' ')) goto reset;
    skip_space(p);

    if (!consume_byte(p, 'R')) goto reset;

    res = true;

reset:
    goto_offset(p, pos);
    return res;
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

    return (Reference) {
        .object_num = obj_ref,
            .generation = num,
    };
}

Stream parse_stream(Parser *p) {
    PRINT_PARSE_FN();

    EXPECT_BYTES(p, "stream");
    skip_space(p);

    u64 start = cursor_pos(p);
    ADVANCE_IF(p, !CURR_BYTES(p, "endstream"));


    u64 end = cursor_pos(p);

    EXPECT_BYTES(p, "endstream");

    return (Stream) {
        .slice = (PDFSlice) {
            .ptr = p->buffer + start,
                .len = end - start,
        }
    };
}

// Integer or Real
PDFObject parse_number(Parser *p) {

    // TODO: .02
    i8 sign = 1;
    if (consume_byte(p, '-')) {
        sign = -1;
    }
    consume_byte(p, '+');

    u64 uint = parse_uint(p);

    if (sign == -1) {
        GB_ASSERT(uint < I64_MAX);
    }
    i64 int_part = uint;

    if (consume_byte(p, '.')) {
        u64 frac_part = parse_uint(p);

        u64 scale = 1;
        u64 tmp = frac_part;
        while (tmp > 0) {
            scale *= 10;
            tmp /= 10;
        }

        f64 result = (float)int_part + (float)frac_part / scale;
        result *= sign;
        return obj_from_real_number((RealNumber) {.value = result, });
    } else {
        return obj_from_integer((Integer) {.value = int_part, });
    }
}

PDFObject parse_object(Parser *p);

// e.g [ 50 30 /Fred ]
ObjectArray parse_array(Parser *p) {
    PRINT_PARSE_FN();
    ObjectArray array = {0};

    EXPECT_BYTE(p, '[');

    for (;;) {
        skip_space(p);
        if (p->curr_byte == ']') break;

        PDFObject object = parse_object(p);
        arrput(array.data, object);
        array.count += 1;
    }

    EXPECT_BYTE(p, ']');

    return array;
}

// e.g << /Three 3 /Five 5 >>
Dictionary parse_dictionary(Parser *p) {
    PRINT_PARSE_FN();

    Dictionary dict = {0};

    EXPECT_BYTES(p, "<<");

    for (;;) {
        skip_space(p);
        if (p->curr_byte == '>') break;

        Name name = parse_name(p);
        PDFObject object = parse_object(p);

        DictionaryEntry entry = {
            .name = name,
            .object = object,
        };
        arrput(dict.entries, entry);
        dict.count += 1;
    }

    EXPECT_BYTES(p, ">>");

    return dict;
}

local inline StreamData parse_stream_data(Stream s) {
    FilterKind filter = NO_FILTER;

    for (u64 i = 0; i < s.dict.count; i++) {
        DictionaryEntry *e = &s.dict.entries[i];
        Name n = e->name;

        if (cmp_name_str(n, "Filter")) {
            if (e->object.kind != OBJ_NAME) GB_PANIC("Filter value is not a name!");
            Name filter_name = e->object.data.name;

            if (cmp_name_str(filter_name, "FlateDecode")) filter = FLATE_DECODE;
            else {
                printf("unknown filter: ");
                print_name(filter_name);
                printf("\n");
                GB_PANIC("unknown filter\n");
            }

        } else if (cmp_name_str(n, "Length")) {
            //TODO
        } 
    }

    switch (filter) {
        case FLATE_DECODE:  return inflate_stream(&s);
        case NO_FILTER:     return (StreamData) {
                                .ptr = s.slice.ptr,
                                .len = s.slice.len,
                                .decompressed = false 
                            };

        default: GB_PANIC("unhandled FilterKind: %u", filter);
    }
}

PDFObject parse_object(Parser *p) {

    PRINT_PARSE_FN();
    skip_space(p);


    if (CURR_BYTE(p, '/')) {
        Name name = parse_name(p);
        return obj_from_name(name);

    } else if (isdigit(p->curr_byte)) {
        if (is_reference(p)) {
            Reference ref = parse_reference(p);
            return obj_from_reference(ref);

        }  else {
            return parse_number(p);
        }

    } else if (CURR_BYTE(p, '-') || CURR_BYTE(p, '+')) {
        return parse_number(p);

    } else if (CURR_BYTE(p, '(')) {
        PDFString string = parse_string(p);
        return obj_from_string(string);

    } else if (CURR_BYTES(p, "<<")) {
        Dictionary dict = parse_dictionary(p);
        skip_space(p);

        if (CURR_BYTES(p, "stream")) {
            Stream s = parse_stream(p);
            s.dict = dict;

            FilterKind filter = NO_FILTER;

            s.data = parse_stream_data(s);


            return obj_from_stream(s);
        } else {
            return obj_from_dictionary(dict);
        }

    } else if (CURR_BYTE(p, '<')) {
        HexString hex_str = parse_hex_string(p);
        return obj_from_hex_string(hex_str);

    } else if (CURR_BYTE(p, '[')) {
        ObjectArray dict = parse_array(p);
        return obj_from_array(dict);

    } else if (CONSUME_BYTES(p, "true")) {
        Boolean boolean = { .value = true };
        return obj_from_boolean(boolean);
    } else if (CONSUME_BYTES(p, "false")) {
        Boolean boolean = { .value = false };
        return obj_from_boolean(boolean);
    } else if (CONSUME_BYTES(p, "null")) {
        return obj_from_pdf_null((PDFNull) { 0 });
    } else if (CONSUME_BYTES(p, "obj")) {

        PDFObject obj = { 0 };
        while (!CURR_BYTES(p, "endobj")) {
            obj = parse_object(p);
            skip_space(p);
        }

        return obj;
    } else {
        println("failed at:");
        print_next_n_bytes(p, 30);
        println(" ");
        GB_PANIC("unknown object type! (starts with %c / %u)", p->curr_byte, (u8)p->curr_byte);
    }
}

// entry 20 bytes long
// xxxxxxxxxx zzzzz z eol
// 1          2     3
//
// 1: 10-digit byte offset
// 2: 5-digit generation number
// 3: in-use 'n' or free 'f'
//

XRefEntry parse_xref_entry(Parser *p) {
    PRINT_PARSE_FN();
    GB_ASSERT_MSG(fwd_n_bytes(p, 20), "invalid xref entry: too short");

    // jump back to start
    back_n_bytes(p, 20);

    u64 byte_offset = parse_uint_len(p, 10);
    EXPECT_BYTE(p, ' ');
    u32 generation = (u32)parse_uint_len(p, 5);
    EXPECT_BYTE(p, ' ');
    bool in_use = (p->curr_byte == 'n');
    next_byte(p);
    skip_space(p);

    return (XRefEntry) {
        .byte_offset = byte_offset,
            .in_use = in_use,
    };
}

XRefTable parse_xref_table(Parser *p) {
    PRINT_PARSE_FN();

    EXPECT_BYTES(p, "xref");
    skip_space(p);
    u32 obj_id = (u32)parse_uint(p);
    skip_space(p);
    // skip first entry
    u64 obj_count = parse_uint(p) - 1;
    skip_space(p);
    parse_xref_entry(p);

    XRefEntry *entries = malloc(obj_count * sizeof(XRefEntry));
    PDFObject *objects = malloc(obj_count * sizeof(PDFObject));

    for (u64 i = 0; i < obj_count; i++) {
        XRefEntry e = parse_xref_entry(p);
        entries[i] = e;
    }

    for (u64 i = 0; i < obj_count; i++) {
        u64 offset = entries[i].byte_offset;
        goto_offset(p, offset);

        u64 id = parse_uint(p);
        GB_ASSERT(id - 1 == i);
        skip_space(p);
        u64 gen = parse_uint(p);
        skip_space(p);

        PDFObject obj = parse_object(p);
        objects[i] = obj;
    }

    return (XRefTable) {
        .obj_id = obj_id,
            .obj_count = obj_count,
            .entries = entries,
            .objects = objects,
    };

}


Parser make_parser(u8 *content, u64 size) {
    GB_ASSERT_MSG(size != 0, "empty pdf found");
    GB_ASSERT_MSG(content != NULL, "empty pdf found");

    return (Parser) {
        .buffer = content,
            .size = size,
            .cursor = 0,
            .curr_byte = content[0],
    };
}

// e.g
// %PDF-1.4
// .... (9 bytes)
void parse_header(Parser *p) {
    PRINT_PARSE_FN();
    PDFSlice header = parse_comment(p);
    GB_ASSERT_MSG(fwd_n_bytes(p, 9), "invalid pdf header");
}

PDFTrailer parse_trailer(Parser *p) {
    goto_eof(p);

    while (prev_byte(p)) {
        if (CONSUME_BYTES(p, "trailer")) {
            skip_space(p);
            Dictionary dict = parse_dictionary(p);
            skip_space(p);
            EXPECT_BYTES(p, "startxref");
            skip_space(p);
            u64 table_offset = parse_uint(p);

            return (PDFTrailer) { .xref_table_offset = table_offset, .dict = dict, };
        }
    }
    GB_PANIC("Could not find trailer, reached start of file");
}

#define COUNT_N_PARSED_BYTES(parse_fn) \
    u64 __start = p.curr_byte_indx; \
    parse_fn; \
    p.parsed_pdf.n_bytes_parsed += p.curr_byte_indx - __start;

PDF parse_pdf(PDFContent *content) {
    PDF pdf = {0};

    Parser parser = make_parser(content->data, content->size);
    Parser *p = &parser;

    pdf.content = *content;
    *content = (PDFContent){0};

    PDFTrailer trailer = parse_trailer(p);

    goto_offset(p, trailer.xref_table_offset);
    XRefTable table = parse_xref_table(p);

    pdf.xref_table = table;
    pdf.trailer = trailer;

    return pdf;
};

PDFContent load_file(const char *path) {
    /* u64 _size = 0; */
    /* u8 *_buffer = NULL; */

    u8 *source = NULL;
    u64 bufsize = 0;
    // SET_BIN_MODE?
    FILE *fp = fopen(path, "rb");
    GB_ASSERT_MSG(fp, "could not open file: %s", path);

    /* Go to the end of the file. */
    if (fseek(fp, 0L, SEEK_END) == 0) {

        bufsize = ftell(fp);
        GB_ASSERT_MSG(bufsize != -1, "could not tell buffer size");

        /* Allocate our buffer to that size. */
        source = malloc(sizeof(char) * (bufsize + 1));
        GB_ASSERT(source);
        memset(source, 0, bufsize + 1);

        /* Go back to the start of the file. */
        GB_ASSERT(fseek(fp, 0L, SEEK_SET) == 0);

        /* read into memory */
        usize newLen = fread(source, sizeof(u8), bufsize, fp);

        GB_ASSERT_MSG(ferror(fp) == 0, "Error reading file" );

        source[newLen++] = '\0';
    }

    fclose(fp);

    return (PDFContent) {
        .data = source,
            .size = bufsize,
    };
}
