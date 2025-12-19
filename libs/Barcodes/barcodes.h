#ifndef BARCODE_H
#define BARCODE_H

#include "hpdf.h"

typedef enum {
    BARCODE_CODE128,
    BARCODE_EAN13,
    BARCODE_UPCA
} BarcodeType;

// Barcode validation
int validate_barcode_data(BarcodeType type, const char* data);

// Main barcode drawing function
void draw_barcode(HPDF_Page page, float x, float y, float width, float height, 
                  BarcodeType type, const char* data);

// Individual barcode type drawing functions
void draw_code128(HPDF_Page page, float x, float y, float width, float height, const char* data);
void draw_ean13(HPDF_Page page, float x, float y, float width, float height, const char* data);
void draw_upca(HPDF_Page page, float x, float y, float width, float height, const char* data);

#endif