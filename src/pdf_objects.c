#include "pdf_objects.h"

#include <ctype.h>

bool cmp_name_str(Name n, const char *str) {
    u32 len = strlen(str);
    if (n.slice.len != len) return false;

    for (u32 i = 0; i < len; i++) {
        if (n.slice.ptr[i] != str[i]) return false;
    }

    return true;
}

DictionaryEntry *find_dict_entry(const Dictionary *dict, const char *str) {
    DictionaryEntry *ret = NULL;


    for (u64 i = 0; i < dict->count; i++) {
        DictionaryEntry *entry = &dict->entries[i];

        if (cmp_name_str(entry->name, str)) {
            ret = entry;
            break;
        }
    }

    return ret;
}

DictionaryEntry *get_dict_entry(const Dictionary *dict, const char *str) {
    DictionaryEntry *ret = find_dict_entry(dict, str);
    ASSERT_MSG(ret, "could not find dict entry: %s", str);
    return ret;
}


#define X(TYP, VAR, IDNT)               \
PDFObject obj_from_##IDNT(TYP IDNT) {   \
    return (PDFObject) {                \
        .kind = OBJ_##VAR,              \
        .data = (union PDFObjectData) { \
        .IDNT = IDNT,                   \
        },                              \
    };                                  \
}                                       
X_PDF_OBJECTS
#undef X

void print_buffer(Buffer b) {
    /* printf("%.*s", (u32)b.len, b.data); */ 
    for (u64 i = 0; i < b.size; i++) {
        char c = b.data[i];
        if (isprint(c)) {
            printf("%c", c); 
        } else if (c == '\n' || c == '\r') {
            printf("\n");
        } else {
            printf("\\x%02x", c);
        }
    }
}

void print_pdf_slice(PDFSlice c) {
    printf("%.*s", (u32)c.len, c.ptr); 
}

void print_name(Name name) {
    print_pdf_slice(name.slice);
}

void print_string(PDFString string) {
    print_pdf_slice(string.slice);
}

void print_hex_string(HexString hex_str) {
    print_pdf_slice(hex_str.slice);
}

void print_boolean(Boolean boolean) {
    if (boolean.value) {
        printf("true");
    } else {
        printf("false");
    }
}

void print_integer(Integer integer) {
    printf("%li", integer.value);
}

void print_real_number(RealNumber real) {
    printf("%f", real.value);
}

void print_reference(Reference ref) {
    printf("%lu %lu R", ref.object_num, ref.generation);
}

void print_array(ObjectArray array) {
    printf("[ ");
    for (u64 i = 0; i < array.count; i++) {
        print_object(array.data[i]);
        printf(" ");
    }
    printf("]");
}

void print_dict_entry(DictionaryEntry e) {
    print_name(e.name);
    printf(" ");
    print_object(e.object);
}

void print_dictionary(Dictionary dict) {
    printf("<< ");

    for (u64 i = 0; i < dict.count; i++) {
        printf("(");
        print_name(dict.entries[i].name);
        printf(", ");
        print_object(dict.entries[i].object);
        printf(") ");
    }

    printf(">>");
}

void print_stream(Stream s) {
    printf("{ stream");
    print_dictionary(s.dict);
    printf(" }");
}

void print_decoded_stream(DecodedStream s) {
    printf("{ decoded_stream");

    switch (s.kind) {
    case STREAM_DATA_BUFFER: printf("Buffer: %lu", s.data.buffer.size); break;
    case STREAM_DATA_IMAGE: printf("image: %u x %u", s.data.image.width, s.data.image.height); break;
    case STREAM_DATA_NONE: printf("raw buffer"); break;
    default: PANIC("unhandled StreamDataKind: found: %i", s.kind); break;
    }

    printf(" }");
}

void print_null(PDFNull n) {
    printf("null");
}

const char *obj_kind_to_str(enum PDFObjectKind kind) {
    switch (kind) {
        #define X(TYP, VAR, IDNT) case OBJ_##VAR : return #VAR;
        X_PDF_OBJECTS
        #undef X
        default: PANIC("unknown object kind: %u", kind);
    }
}

void print_object_kind(enum PDFObjectKind kind) {
    printf("%s", obj_kind_to_str(kind));
}

void print_object(PDFObject o) {
    switch (o.kind) {
        case OBJ_NULL           : print_null(o.data.pdf_null); break;
        case OBJ_INTEGER        : print_integer(o.data.integer); break;
        case OBJ_REAL_NUMBER    : print_integer(o.data.integer); break;
        case OBJ_BOOLEAN        : print_boolean(o.data.boolean); break;
        case OBJ_NAME           : print_name(o.data.name); break;
        case OBJ_STRING         : print_string(o.data.string); break;
        case OBJ_HEX_STRING     : print_hex_string(o.data.hex_string); break;
        case OBJ_REFERENCE      : print_reference(o.data.reference); break;
        case OBJ_ARRAY          : print_array(o.data.array); break;
        case OBJ_DICTIONARY     : print_dictionary(o.data.dictionary); break;
        case OBJ_STREAM         : print_stream(o.data.stream); break;
        case OBJ_DECODED_STREAM : print_decoded_stream(o.data.decoded_stream); break;
        default: PANIC("unhandled object kind: %s", obj_kind_to_str(o.kind));
    }
}

void print_xref_entry(XRefEntry e) {
    printf("%010lu %c", e.byte_offset, e.in_use ? 'n' : 'f');
}

void print_xref_table(XRefTable t) {
    printf("xref\n");
    printf("%u %u\n", t.obj_id, t.obj_count);
    for (u64 i = 0; i < t.obj_count; i++) {
        print_xref_entry(t.entries[i]);
        println(" ");
    } 

}

void print_pdf(PDF pdf) {
    print_xref_table(pdf.xref_table);

    // object buffer
    for (u64 i = 0; i < pdf.xref_table.obj_count; i++) {
        printf("\nobj: %lu\n", i + 1);
        print_object(pdf.object_buffer[i]);
        printf("\nendobj\n");
    } 
}

local inline void free_buffer(Buffer b) {
    free(b.data);
}

local inline void free_image(RawImage img) {
    free(img.data);
}

void free_object(PDFObject *obj);

local inline void free_dictionary(Dictionary dict) {
    for (u64 i = 0; i < dict.count; i++) {
        free_object(&dict.entries[i].object);
    }
    arrfree(dict.entries);
}

local inline void free_stream(Stream s) {
    free_dictionary(s.dict);
}

local inline void free_decoded_stream(DecodedStream ds) {
    switch (ds.kind) {
    case STREAM_DATA_BUFFER: { free_buffer(ds.data.buffer); break; }
    case STREAM_DATA_IMAGE:  { free_image(ds.data.image); break; }

    case STREAM_DATA_NONE:
      break;
    }

    free_stream(ds.raw_stream);
}

local inline void free_array(ObjectArray arr) {
    for (u64 i = 0; i < arr.count; i++) {
        free_object(&arr.data[i]);
    }
    arrfree(arr.data);
}

void free_object(PDFObject *obj) {
    switch (obj->kind) {
        case OBJ_ARRAY          : { free_array(obj->data.array); break; }
        case OBJ_DICTIONARY     : { free_dictionary(obj->data.dictionary); break; }
        case OBJ_STREAM         : { free_stream(obj->data.stream); break; }
        case OBJ_DECODED_STREAM : { free_decoded_stream(obj->data.decoded_stream); break; }

        case OBJ_NULL:
        case OBJ_NAME:
        case OBJ_INTEGER:
        case OBJ_REAL_NUMBER:
        case OBJ_BOOLEAN:
        case OBJ_STRING:
        case OBJ_HEX_STRING:
        case OBJ_REFERENCE:
          break;
        }

    *obj = (PDFObject) {0};
}

local inline void free_xref_table(XRefTable *t) {
    ASSERT(t->entries);
    
    free(t->entries);

    *t = (XRefTable){0};
}

local inline void free_pdf_content(PDFContent *c) {
    ASSERT(c->data);
    free(c->data);
    *c = (PDFContent){0};
}

void free_pdf(PDF *pdf) {
    free_dictionary(pdf->trailer.dict);
    free_xref_table(&pdf->xref_table);
    free(pdf->object_buffer);
    free_pdf_content(&pdf->content);
}
