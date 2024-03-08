#include "utils.h"

#include "pdf_objects.h"

struct JPEGImage {
    u32 n_components;
    u64 width;
    u64 height;
    u8 *data;
};


StreamData inflate_stream(PDFSlice);
StreamData dct_stream(PDFSlice);
