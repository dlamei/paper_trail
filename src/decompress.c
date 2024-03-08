#include "decompress.h"


#include <jpeglib.h>
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

DecodedStream inflate_decode(Stream *stream) {
	i32 ret = Z_ERRNO;
	u8 *src = stream->slice.ptr;
	u64 len = stream->slice.len;

	z_stream strm = { 0 };
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


    Buffer buffer = {
        .data = out,
        .size = out_len,
    };

    return (DecodedStream) {
        .data = (union StreamData) { .buffer = buffer },
        .kind = STREAM_DATA_BUFFER,
        .raw_stream = *stream,
    };

zerr:
	PANIC("ZERROR: %s", zret_to_str(ret));
}


DecodedStream dct_decode(Stream *stream) {
    u32 width = 0;
    u32 height = 0;
    u64 size = 0;
    u32 n_channels = 0;
    u8 *data = NULL;

    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr err;

    info.err = jpeg_std_error(&err);
    jpeg_create_decompress(&info);

    jpeg_mem_src(&info, stream->slice.ptr, stream->slice.len);
    (void)jpeg_read_header(&info, true);
    (void)jpeg_start_decompress(&info);

    width = info.output_width;
    height = info.output_height;
    n_channels = info.num_components;

    size = width * height * n_channels;
    u32 row_stride = width * n_channels;
    data = malloc(size * sizeof(u8));

    u8 *out_scanlines_ptr[1] = {0}; // for now only 1 scanline at the time
    while (info.output_scanline < info.output_height) {
        out_scanlines_ptr[0] = &data[row_stride * info.output_scanline];
        (void) jpeg_read_scanlines(&info, out_scanlines_ptr, 1);
    }

    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);

    RawImage image = {
        .data = data,
        .width = width,
        .height = height,
        .n_channels = n_channels,
    };

    return (DecodedStream) {
        .data = (union StreamData) { .image = image },
        .kind = STREAM_DATA_IMAGE,
        .raw_stream = *stream,
    };
}
