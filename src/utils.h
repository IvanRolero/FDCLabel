#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include "cJSON.h"
#include "hpdf.h"
#include "qrcodegen.h"
#include <ctype.h>
#include "barcodes.h"

/* ---------- Security Configuration ---------- */
#define HEX_LENGTH          10
#define MAX_TEXT_LEN        1024
#define MAX_FIELD_LEN       1024
#define MAX_CSV_LINE_LEN    8192
#define MAX_CSV_FIELDS      256
#define MAX_CSV_ROWS        100000
#define MAX_CONFIG_SIZE     (10 * 1024 * 1024)
#define MAX_FIELD_COUNT     1000
#define MAX_LINE_COUNT      1000
#define MAX_CUSTOM_FONTS    100

/* ---------- Types ---------- */
typedef struct {
    float x_start, x_end, y_start, y_end;
    char text[MAX_TEXT_LEN];
    float font_size;
    char font_name[64];
    int wrap;
    int align;
    int max_length;
} Field;

typedef enum { LINE_RAW, LINE_H_TRANSFORM } LineType;

typedef struct {
    LineType type;
    float x_start, y_start, x_end, y_end;
    float y;
    float width;
} LineEntry;

typedef struct {
    char name[64];
    char file[256];
    char encoding[64];
} CustomFont;

typedef struct {
    char default_font[64];
    CustomFont *custom_fonts;
    int custom_font_count;
} FontConfig;

typedef struct {
    HPDF_PageSizes size;
    HPDF_PageDirection orientation;
    float line_width;
} PageConfig;

typedef struct {
    float x, y, width, height;
    char text[MAX_TEXT_LEN];
    char type[16];  // "code128", "ean13", "upca"
} BarcodeEntry;

typedef struct {
    float x;
    float y;
    float size;
    char text[MAX_FIELD_LEN];
    int enabled;  // Add this to make QR codes optional
} QRCodeEntry;

/* ---------- CSV Types ---------- */
typedef struct {
    char **fields;
    int count;
} CSVRow;

typedef struct {
    CSVRow *rows;
    char **field_names;
    int field_count;
    int row_count;
} CSVData;

/* ---------- Function Declarations ---------- */
// Safe string functions
size_t safe_strncpy(char *dest, const char *src, size_t dest_size);
int safe_atoi(const char *str, int default_val);

// Helpers
void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data);
void generate_hex_code(char *hex, int length);

// CSV functions
CSVData* parse_csv(const char *filename);
void free_csv_data(CSVData *csv);


int load_barcodes_from_json(cJSON *root, BarcodeEntry **out_barcodes, int *out_count, 
                           const char *hex_code, CSVData *csv, int csv_row_index);
void draw_barcode_entry(HPDF_Page page, BarcodeEntry *barcode);

// Drawing functions
void draw_qr_code(HPDF_Page page, float x, float y, float size, const char *text);
void draw_text_in_box(HPDF_Page page, HPDF_Font font,
                    float x_start, float x_end, float y_start, float y_end,
                    const char *text, float font_size, int align);

// JSON loading functions
int parse_align(const char *s);
HPDF_PageSizes parse_page_size(const char *s);
HPDF_PageDirection parse_orientation(const char *s);
int load_page_config_from_json(cJSON *root, PageConfig *config);
int load_fonts_from_json(cJSON *root, FontConfig *font_config, HPDF_Doc pdf);
int load_fields_from_json(cJSON *root, Field **out_fields, int *out_count, 
                         const char *hex_code, CSVData *csv, int csv_row_index);
int load_lines_from_json(cJSON *root, LineEntry **out_lines, int *out_count);
int load_qr_from_json(cJSON *root, QRCodeEntry *qr_entry, const char *hex_code, CSVData *csv, int csv_row_index);
int validate_json_config(cJSON *root);

// Command line and validation
void print_version();
void print_help(const char *program_name);
int validate_config_only(const char *config_filename);

#endif