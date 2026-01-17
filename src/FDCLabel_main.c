/* FDCLabel.c Fast Dynamic C Label Generator
 *
 * Copyright (C) Ivan Rolero
 *
 * This file is part of FDCLabel.
 *
 * FDCLabel is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * FDCLabel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include "cJSON.h"
#include "hpdf.h"
#include "qrcodegen.h"
#include "barcodes.h"
#include <ctype.h>
#include "utils.h"

// Safe string functions
size_t safe_strncpy(char *dest, const char *src, size_t dest_size);
int safe_atoi(const char *str, int default_val);

// Helpers
void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data);
void generate_hex_code(char *hex, int length);

// CSV functions
CSVData* parse_csv(const char *filename);
void free_csv_data(CSVData *csv);

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
int load_fields_from_json(cJSON *root, Field **out_fields, int *out_count, const char *hex_code, CSVData *csv, int csv_row_index);
int load_lines_from_json(cJSON *root, LineEntry **out_lines, int *out_count);
int load_qr_from_json(cJSON *root, QRCodeEntry *qr_entry, const char *hex_code, CSVData *csv, int csv_row_index);
int validate_json_config(cJSON *root);

// Command line and validation
void print_version();
void print_help(const char *program_name);
int validate_config_only(const char *config_filename);

//Main
int main(int argc, char **argv) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    // Default values
    const char *csv_filename = NULL;
    const char *config_filename = "config.json";
    const char *output_filename = "labels.pdf";
    int specific_row = -1;
    int validate_only = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        }
        else if (strcmp(argv[i], "--validate") == 0) {
            validate_only = 1;
        }
        else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) && i+1 < argc) {
            config_filename = argv[++i];
        }
        else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) && i+1 < argc) {
            output_filename = argv[++i];
        }
        else if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--row") == 0) && i+1 < argc) {
            specific_row = safe_atoi(argv[++i], -1);
            if (specific_row < 0) {
                fprintf(stderr, "Warning: Row index cannot be negative, using 0\n");
                specific_row = 0;
            }
        }
        else if (argv[i][0] != '-') {
            // Positional argument (CSV file)
            if (!csv_filename) {
                csv_filename = argv[i];
            } else {
                fprintf(stderr, "Error: Unexpected argument: %s\n", argv[i]);
                print_help(argv[0]);
                return 1;
            }
        }
        else {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_help(argv[0]);
            return 1;
        }
    }
    
    // Validate we have required arguments
    if (!csv_filename && !validate_only) {
        fprintf(stderr, "Error: CSV file is required\n");
        print_help(argv[0]);
        return 1;
    }
    
    // Handle validate-only mode
    if (validate_only) {
        return validate_config_only(config_filename);
    }
    
    srand((unsigned int)time(NULL));
    
    // Parse CSV
    CSVData *csv = parse_csv(csv_filename);
    if (!csv) {
        fprintf(stderr, "Failed to parse CSV file: %s\n", csv_filename);
        return 1;
    }
    
    printf("Loaded CSV '%s' with %d fields and %d rows\n", csv_filename, csv->field_count, csv->row_count);
    printf("Using config: %s\n", config_filename);
    printf("Output file: %s\n", output_filename);
    
    // Load and parse config
    FILE *f = fopen(config_filename, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open config file: %s\n", config_filename);
        free_csv_data(csv);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size > MAX_CONFIG_SIZE) {
        fprintf(stderr, "Error: Config file too large: %ld bytes (max: %d)\n", size, MAX_CONFIG_SIZE);
        fclose(f);
        free_csv_data(csv);
        return 1;
    }
    rewind(f);
    
    char *data = (char*)malloc(size + 1);
    if (!data) {
        fprintf(stderr, "Memory allocation error\n");
        fclose(f);
        free_csv_data(csv);
        return 1;
    }
    
    size_t read_size = fread(data, 1, size, f);
    if (read_size != (size_t)size) {
        fprintf(stderr, "Error reading config file\n");
        free(data);
        fclose(f);
        free_csv_data(csv);
        return 1;
    }
    
    data[size] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(data);
    free(data);
    
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            fprintf(stderr, "Error parsing JSON config file '%s' before: %s\n", config_filename, error_ptr);
        }
        free_csv_data(csv);
        return 1;
    }

    if (validate_json_config(root) != 0) {
        fprintf(stderr, "Invalid JSON configuration in '%s'\n", config_filename);
        free_csv_data(csv);
        cJSON_Delete(root);
        return 1;
    }

    PageConfig page_config;
    if (load_page_config_from_json(root, &page_config) != 0) {
        fprintf(stderr, "Error loading page config from JSON, using defaults\n");
        page_config.size = HPDF_PAGE_SIZE_A4;
        page_config.orientation = HPDF_PAGE_LANDSCAPE;
        page_config.line_width = 3.0f;
    }

    HPDF_Doc pdf = HPDF_New(error_handler, NULL);
    if (!pdf) {
        fprintf(stderr, "Error creating PDF\n");
        free_csv_data(csv);
        cJSON_Delete(root);
        return 1;
    }
    HPDF_UseUTFEncodings(pdf);

    HPDF_SetCompressionMode(pdf, HPDF_COMP_ALL);

    FontConfig font_config;
    if (load_fonts_from_json(root, &font_config, pdf) != 0) {
        fprintf(stderr, "Warning: Could not load font configuration, using defaults\n");
        safe_strncpy(font_config.default_font, "Helvetica-Bold", sizeof(font_config.default_font));
        font_config.custom_fonts = NULL;
        font_config.custom_font_count = 0;
    }

    // Determine which rows to process
    int start_row = 0;
    int end_row = csv->row_count - 1;
    
    if (specific_row >= 0) {
        start_row = specific_row;
        if (start_row >= csv->row_count) {
            fprintf(stderr, "Warning: Row %d is beyond CSV row count (%d), using last row\n", 
                    specific_row, csv->row_count - 1);
            start_row = csv->row_count - 1;
        }
        end_row = start_row;
        printf("Processing row %d only\n", start_row);
    } else {
        printf("Processing all %d rows\n", csv->row_count);
    }

    for (int row_index = start_row; row_index <= end_row; row_index++) {
        char hex_code[HEX_LENGTH + 1];
        generate_hex_code(hex_code, HEX_LENGTH);

        Field *fields = NULL;
        int field_count = 0;
        if (load_fields_from_json(root, &fields, &field_count, hex_code, csv, row_index) != 0) {
            fprintf(stderr, "Error loading fields from JSON for row %d\n", row_index);
            continue;
        }

        LineEntry *lines = NULL;
        int line_count = 0;
        if (load_lines_from_json(root, &lines, &line_count) != 0) {
            fprintf(stderr, "Error loading lines from JSON\n");
            free(fields);
            continue;
        }

        // Load QR code configuration (optional)
        QRCodeEntry qr_code;
        if (load_qr_from_json(root, &qr_code, hex_code, csv, row_index) != 0) {
            fprintf(stderr, "Error loading QR code configuration\n");
            // Initialize with disabled state
            qr_code.enabled = 0;
        }

        HPDF_Page page = HPDF_AddPage(pdf);
        if (!page) {
            fprintf(stderr, "Error creating PDF page\n");
            free(fields);
            free(lines);
            continue;
        }
        
        HPDF_Page_SetSize(page, page_config.size, page_config.orientation);
        HPDF_Page_SetLineWidth(page, page_config.line_width);

        for (int i = 0; i < line_count; ++i) {
            // Set individual line width BEFORE drawing each line
            HPDF_Page_SetLineWidth(page, lines[i].width);
            
            if (lines[i].type == LINE_H_TRANSFORM) {
                float y_pos = lines[i].y;
                HPDF_Page_MoveTo(page, lines[i].x_start, y_pos);
                HPDF_Page_LineTo(page, lines[i].x_end, y_pos);
                HPDF_Page_Stroke(page);
            } else {
                HPDF_Page_MoveTo(page, lines[i].x_start, lines[i].y_start);
                HPDF_Page_LineTo(page, lines[i].x_end, lines[i].y_end);
                HPDF_Page_Stroke(page);
            }
        }
        // Draw QR code only if enabled and has text
        if (qr_code.enabled && strlen(qr_code.text) > 0) {
            draw_qr_code(page, qr_code.x, qr_code.y, qr_code.size, qr_code.text);
        }       

        BarcodeEntry *barcodes = NULL;
        int barcode_count = 0;
        if (load_barcodes_from_json(root, &barcodes, &barcode_count, hex_code, csv, row_index) == 0) {
            for (int i = 0; i < barcode_count; ++i) {
                draw_barcode_entry(page, &barcodes[i]);
            }
            free(barcodes);
        }


        for (int i = 0; i < field_count; ++i) {
            float fx0 = fields[i].x_start;
            float fx1 = fields[i].x_end;
            float fy0 = fields[i].y_start;
            float fy1 = fields[i].y_end;

            // Validate coordinates
            if (fx1 <= fx0 || fy1 <= fy0) continue;

            HPDF_Font field_font;
            if (strlen(fields[i].font_name) > 0) {
                int found = 0;
                for (int j = 0; j < font_config.custom_font_count; j++) {
                    if (strcmp(font_config.custom_fonts[j].name, fields[i].font_name) == 0) {
                        field_font = HPDF_GetFont(pdf, font_config.custom_fonts[j].name, "WinAnsiEncoding");
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    field_font = HPDF_GetFont(pdf, fields[i].font_name, "WinAnsiEncoding");
                }
            } else {
                field_font = HPDF_GetFont(pdf, font_config.default_font, "WinAnsiEncoding");
            }

            if (!field_font) {
                field_font = HPDF_GetFont(pdf, font_config.default_font, "WinAnsiEncoding");
                if (!field_font) continue;
            }

            if (fields[i].wrap) {
                draw_text_in_box(page, field_font, fx0, fx1, fy0, fy1, 
                               fields[i].text, fields[i].font_size, fields[i].align);
            } else {
                HPDF_Page_BeginText(page);
                HPDF_Page_SetFontAndSize(page, field_font, fields[i].font_size);
                float x_offset = fx0 + 5.0f;
                if (fields[i].align == 1) {
                    float lw = HPDF_Page_TextWidth(page, fields[i].text);
                    float boxw = fields[i].x_end - fields[i].x_start - 10.0f;
                    x_offset = fx0 + (boxw - lw) / 2.0f;
                } else if (fields[i].align == 2) {
                    float lw = HPDF_Page_TextWidth(page, fields[i].text);
                    x_offset = fx1 - lw - 5.0f;
                }
                HPDF_Page_TextOut(page, x_offset, fy1 - fields[i].font_size - 5.0f, fields[i].text);
                HPDF_Page_EndText(page);
            }
        }

        free(fields);
        free(lines);
        
        printf("Generated label for row %d\n", row_index);
    }  

    if (HPDF_SaveToFile(pdf, output_filename) != HPDF_OK) {
        fprintf(stderr, "Error saving PDF to: %s\n", output_filename);
    } else {
        printf("Successfully generated: %s with %d labels\n", output_filename, (end_row - start_row + 1));
    }

    HPDF_Free(pdf);
    if (font_config.custom_fonts) {
        free(font_config.custom_fonts);
    }
    free_csv_data(csv);
    cJSON_Delete(root);
    return 0;
}
