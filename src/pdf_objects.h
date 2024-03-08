#pragma once

#include "utils.h"


typedef struct Buffer {
    u8 *data;
    u64 size;
} Buffer;

struct PDFObject;

// Buffer holding the content of the PDF file
typedef struct PDFContent {
    u8 *data;
    u64 size;
} PDFContent;

// slice of a PDFContent
typedef struct PDFSlice {
    u8 *ptr;
    u64 len;
} PDFSlice;

typedef struct PDFNull {
    u8 null; // because some compilers dont allow 0 size structs
} PDFNull;

// e.g (The Quick Brown Fox)
typedef struct PDFString {
    PDFSlice slice;
} PDFString;

typedef struct HexString {
    PDFSlice slice;
} HexString;

// e.g /Name
typedef struct Name {
    PDFSlice slice;
} Name;

typedef struct Reference {
    u64 object_num;
    u64 generation;
} Reference;

// ordered collection of objects, e.g [50, 30, /Fred]
typedef struct ObjectArray {
    struct PDFObject *data;
    usize count;
} ObjectArray;

typedef struct DictionaryEntry DictionaryEntry;

// unordered map from name to object, e.g << /Three 3 /Five 5 >>
typedef struct Dictionary {
    // (/Name Object)
    DictionaryEntry *entries;
    u64 count;
} Dictionary;

typedef struct RawImage {
    u8 *data;
    u32 width;
    u32 height;
    u8 n_channels;
} RawImage;

enum FilterKind {
    FILTER_KIND_NONE,
    FILTER_KIND_FLATE,
    FILTER_KIND_DCT,
    FILTER_KIND_CCITTFAX,
};

enum StreamDataKind {
    STREAM_DATA_BUFFER,
    STREAM_DATA_IMAGE,
    STREAM_DATA_NONE, // only raw_stream
};

union StreamData {
    Buffer buffer;
    RawImage image;
};

// points to the encoded pdf stream
typedef struct Stream {
    Dictionary dict;
    PDFSlice slice;
    enum FilterKind filter_kind;
} Stream;

typedef struct DecodedStream {
    enum StreamDataKind kind;
    union StreamData data;
    Stream raw_stream;

} DecodedStream;

typedef struct Integer {
    i64 value;
} Integer;

typedef struct RealNumber {
    f64 value;
} RealNumber;

typedef struct Boolean {
    bool value;
} Boolean;

// X(type, enum, var_name)
#define X_PDF_OBJECTS            \
    X(PDFNull, NULL, pdf_null)                       \
    X(Name, NAME, name)                              \
    X(Integer, INTEGER, integer)                     \
    X(RealNumber, REAL_NUMBER, real_number)          \
    X(Boolean, BOOLEAN, boolean)                     \
    X(PDFString, STRING, string)                     \
    X(HexString, HEX_STRING, hex_string)             \
    X(Reference, REFERENCE, reference)               \
    X(Dictionary, DICTIONARY, dictionary)            \
    X(Stream, STREAM, stream)                        \
    X(DecodedStream, DECODED_STREAM, decoded_stream) \
    X(ObjectArray, ARRAY, array)                     \

enum PDFObjectKind {
#define X(a, b, c) OBJ_##b,
X_PDF_OBJECTS
#undef X
};

union PDFObjectData {
#define X(a, b, c) a c;
X_PDF_OBJECTS
#undef X
};

typedef struct PDFObject {
    enum PDFObjectKind kind;
    union PDFObjectData data;
} PDFObject;

typedef struct DictionaryEntry {
    Name name;
    PDFObject object;
} DictionaryEntry;

typedef struct XRefEntry {
    u64 byte_offset;
    bool in_use;
    // u32 generation
} XRefEntry;

//TODO: objects not in use
typedef struct XRefTable {
    u32 obj_id;
    u32 obj_count;
    // parsed obj at byte_offset
    PDFObject *objects;
    // byte_offset in_use
    XRefEntry *entries;
} XRefTable;

typedef struct PDFTrailer {
    u64 xref_table_offset;
    Dictionary dict;
} PDFTrailer;

typedef struct PDF {
    PDFContent content;
    //u64 n_bytes_parsed;
    //PDFSlice header;
    PDFTrailer trailer;
    XRefTable xref_table;
} PDF;

#define X(TYP, VAR, IDNT) PDFObject obj_from_##IDNT(TYP IDNT);
X_PDF_OBJECTS
#undef X

PDFObject *derefrence_object(PDFObject *obj, XRefTable table);

bool cmp_name_str(Name n, const char *);

// panics if not found
DictionaryEntry *get_dict_entry(const Dictionary *d, const char *);
// returns null if not found
DictionaryEntry *find_dict_entry(const Dictionary *d, const char *);


void free_pdf(PDF *);


const char *obj_kind_to_str(enum PDFObjectKind kind);

void print_object_kind(enum PDFObjectKind);
void print_pdf_slice(PDFSlice);
void print_buffer(Buffer);
void print_xref_entry(XRefEntry);
void print_xref_table(XRefTable);
void print_pdf_slice(PDFSlice);
void print_array(ObjectArray);
void print_name(Name);
void print_integer(Integer);
void print_real_number(RealNumber);
void print_boolean(Boolean);
void print_string(PDFString);
void print_hex_string(HexString);
void print_reference(Reference);
void print_stream(Stream);
void print_dictionary(Dictionary);
void print_dict_entry(DictionaryEntry);
void print_object(PDFObject);
void print_pdf(PDF);

