#include "pdf_parse.h"


int main() {
    u8 *buffer;
    u64 size;
    defer(free(buffer));

    load_file("../dummy.pdf", &buffer, &size);
    PDF pdf = parse_pdf(buffer, size);

    print_parsed_pdf(pdf);
}

