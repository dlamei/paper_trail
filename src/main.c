#include "pdf_parse.h"

int main() {
    PDFContent file = load_file("../dummy.pdf");
    PDF pdf = parse_pdf(&file);

    /* print_pdf(pdf); */

    free_pdf(&pdf);
}

