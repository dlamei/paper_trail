#pragma once

#include "pdf_objects.h"

PDF parse_pdf(PDFContent *buffer);
PDFContent load_file(const char *path);
