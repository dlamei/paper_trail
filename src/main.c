#include "pdf_parse.h"

int main() {

    PDFContent file = load_file("../dummy.pdf");
    /* PDFContent file = load_file("NUMPDE.pdf"); */
    PDF pdf = parse_pdf(&file);

    free_pdf(&pdf);
}

