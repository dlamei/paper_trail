#include "decompress.h"


#include <jpeglib.h>
#include <setjmp.h>
#include <zlib.h>

#define CHUNK 16384


const char *zret_to_str(i32 ret) {
    switch (ret) {
        case Z_OK: return "OK";
        case Z_ERRNO: return "ERRNO";
        case Z_STREAM_ERROR: return "STREAM_ERROR";
        case Z_DATA_ERROR: return "DATA_ERROR";
        case Z_MEM_ERROR: return "MEM_ERROR";
        case Z_VERSION_ERROR: return "VERSION_ERROR";
        case Z_BUF_ERROR: return "BUF_ERROR";

        case Z_STREAM_END: return "STREAM_END";
        case Z_NEED_DICT: return "Z_NEED_DICT";

        default: PANIC("unknown zret value: %i", ret);
    }
}

local inline bool is_zerr(i32 ret) {
    switch (ret) {
        case Z_ERRNO:
        case Z_STREAM_ERROR:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
        case Z_VERSION_ERROR:
        case Z_BUF_ERROR: return true;

        default: return false;
    }
}

StreamData inflate_stream(PDFSlice slice) {
    i32 ret = Z_ERRNO;
    u8 *src = slice.ptr;
    u64 len = slice.len;

    z_stream strm = {0};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    ret = inflateInit(&strm);
    if (is_zerr(ret)) goto zerr;

    u8 out_buf[CHUNK];

    u8 *out = NULL;
    u64 out_len = 0;

    strm.next_in = src;
    strm.total_in = strm.avail_in = len;

    for (;;) {
        strm.next_out = out_buf;
        strm.avail_out = CHUNK;
        ret = inflate(&strm, Z_NO_FLUSH);
        if (is_zerr(ret)) goto zerr;

        u32 rest = CHUNK - strm.avail_out;

        out_len += rest;
        out = realloc(out, out_len * sizeof(u8));
        memcpy(out + out_len - rest, out_buf, rest);

        if (strm.avail_out != 0) break;
    }

    ASSERT_MSG(ret == Z_STREAM_END, "deflate did not reach the end of the stream: %s", zret_to_str(ret));

    ret = inflateEnd(&strm);
    ASSERT(!is_zerr(ret));


    return (StreamData) {
        .ptr = out,
        .len = out_len,
        .decompressed = true,
    };

zerr:
    PANIC("ZERROR: %s", zret_to_str(ret));
}

jmp_buf jump_buffer;
struct jpeg_error_mgr jerr;

void jpeglib_error_exit(j_common_ptr cinfo) {
    (*cinfo->err->output_message)(cinfo);
    longjmp(jump_buffer, 1);
}


StreamData dct_stream(PDFSlice slice) {
    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr err;

    info.err = jpeg_std_error(&err);
    jpeg_create_decompress(&info);

    u64 width = 0;
    u64 height = 0;
    u32 n_components = 0;
}
