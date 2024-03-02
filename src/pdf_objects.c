#include "pdf_objects.h"

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

void print_dictionary(Dictionary dict) {
    printf("<< ");

    for (u64 i = 0; i < dict.count; i++) {
        print_name(dict.entries[i].name);
        print_object(dict.entries[i].object);
    }

    printf(">>");
}

void print_stream(Stream s) {
    printf("{ stream");
    print_dictionary(s.dict);
    printf(" }");
}

void print_null(PDFNull n) {
    printf("null");
}

const char *obj_kind_to_str(PDFObjectKind kind) {
    switch (kind) {
        #define X(TYP, VAR, IDNT) case OBJ_##VAR : return #VAR;
        X_PDF_OBJECTS
        #undef X
        default: GB_PANIC("unknown object kind: %u", kind);
    }
}

void print_object_kind(PDFObjectKind kind) {
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
        default: GB_PANIC("unhandled object kind: %s", obj_kind_to_str(o.kind));
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

    for (u64 i = 0; i < t.obj_count; i++) {
        printf("\nobj: %lu\n", i + 1);
        print_object(t.objects[i]);
        printf("\nendobj\n");
    } 
}

void print_pdf(PDF pdf) {
    print_xref_table(pdf.xref_table);
}

void free_object(PDFObject *obj);

local inline void free_dictionary(Dictionary *dict) {
    for (u64 i = 0; i < dict->count; i++) {
        free_object(&dict->entries[i].object);
    }
    arrfree(dict->entries);
}

local inline void free_array(ObjectArray *arr) {
    for (u64 i = 0; i < arr->count; i++) {
        free_object(&arr->data[i]);
    }
    arrfree(arr->data);
}

void free_object(PDFObject *obj) {
    switch (obj->kind) {
        case OBJ_ARRAY      : { free_array(&obj->data.array); break; }
        case OBJ_DICTIONARY : { free_dictionary(&obj->data.dictionary); break; }
        case OBJ_STREAM     : { free_dictionary(&obj->data.stream.dict); break; }
        default: break;
    }
}

void free_xref_table(XRefTable *t) {
    GB_ASSERT(t->entries);
    GB_ASSERT(t->objects);

    for (u64 i = 0; i < t->obj_count; i++) {
        free_object(&t->objects[i]);
    }
    
    free(t->entries);
    free(t->objects);

    *t = (XRefTable){0};
}

void free_pdf_content(PDFContent *c) {
    GB_ASSERT(c->data);
    free(c->data);
    *c = (PDFContent){0};
}

void free_pdf(PDF *pdf) {
    free_dictionary(&pdf->trailer.dict);
    free_xref_table(&pdf->xref_table);
    free_pdf_content(&pdf->content);
}
