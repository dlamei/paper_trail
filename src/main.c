#include "pdf_parse.h"

#include "vulkan.h"

#ifdef WIN32
#define MAIN WinMain
#else
#define MAIN main
#endif

int main(void) {

	run();

	//PDFContent file = load_file("../dummy.pdf");
	//PDFContent file = load_file("../NUMPDE.pdf");
	//PDF pdf = parse_pdf(&file);

	//print_pdf(pdf);

	//free_pdf(&pdf);

	return 0;
}

