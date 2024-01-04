#include "pdf_parse.h"

#include "utils.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <pthread.h>

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

inline bool next_byte(Parser *p) {
  if (p->curr_byte_indx + 1 < p->size) {
    p->curr_byte_indx += 1;
    p->curr_byte = p->content[p->curr_byte_indx];
    return true;
  }

  return false;
}

inline bool next_n_bytes(Parser *p, u64 n) {
  if (p->curr_byte_indx + n < p->size) {
    p->curr_byte_indx += n;
    p->curr_byte = p->content[p->curr_byte_indx];
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

inline void expect_byte(Parser *p, u8 byte) {
  GB_ASSERT(p->curr_byte == byte);
  GB_ASSERT_MSG(next_byte(p), "expect_byte called at last byte");
}




String parse_comment(Parser *p) {
  expect_byte(p, '%');

  String c{};
  c.data = p->content + p->curr_byte_indx;

  u64 start_indx = p->curr_byte_indx;

  while (p->curr_byte != '%') {
    GB_ASSERT_MSG(next_byte(p), "unclosing comment, reached EOF");
  }

  c.size = p->curr_byte_indx - start_indx;
  expect_byte(p, '%');

  if (c.size == 0) {
    c.data = NULL;
  }

  return c;
}

inline void skip_space(Parser *p) {
    ADVANCE_IF(p, isspace(p->curr_byte));
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


void parse_xref_table(Parser *p) {
   printf("found xref\n");

   skip_space(p);
   u64 obj_id = parse_uint(p);
   skip_space(p);
   u64 n_objs = parse_uint(p);

   printf("%lu %lu", obj_id, n_objs);
}


inline Parser make_parser(u8 *content, u64 size) {
  GB_ASSERT_MSG(size != 0, "empty pdf found");
  GB_ASSERT_MSG(content != NULL, "empty pdf found");

  return Parser{
      .content = content,
      .size = size,
      .curr_byte_indx = 0,
      .curr_byte = content[0],
      .parsed_pdf{},
  };
}


PDF parse_pdf(u8 *content, u64 size) {
  Parser p = make_parser(content, size);

  if (p.curr_byte == '%') {
    String header = parse_comment(&p);
    print_string(header);
    GB_ASSERT_MSG(next_n_bytes(&p, 9), "invalid pdf header");

    p.parsed_pdf.header = header;
  }

  for (;;) {
    if (p.curr_byte == '%') {
      parse_comment(&p);
    } else if (isalpha(p.curr_byte)) {
      String str = parse_ansi_string(&p);

      if (match_string(str, "xref")) {
          parse_xref_table(&p);
      }
    }

    if (!next_byte(&p))
      break;
  }

  return p.parsed_pdf;
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

