#include "pdf_parse.h"

#include <jpeglib.h>

int main(void) {

    /* PDFContent file = load_file("../dummy.pdf"); */
    PDFContent file = load_file("NUMPDE.pdf");
    PDF pdf = parse_pdf(&file);

    /* print_pdf(pdf); */

    free_pdf(&pdf);

    return 0;
}

