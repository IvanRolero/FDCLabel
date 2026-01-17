/* FDCLabel_utils.c Fast Dynamic C Label Generator
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
#include <errno.h>

/* ---------- Safe String Functions ---------- */
size_t safe_strncpy(char *dest, const char *src, size_t dest_size)
{
    if (!dest || !src || dest_size == 0)
        return 0;

    int written = snprintf(dest, dest_size, "%s", src);

    if (written < 0) {
        dest[0] = '\0';
        return 0;
    }

    return (size_t)written;
}

int safe_atoi(const char *str, int default_val)
{
    if (!str)
        return default_val;

    errno = 0;
    char *end;
    long val = strtol(str, &end, 10);

    if (errno != 0)                 
        return default_val;
    if (end == str || *end != '\0') 
        return default_val;
    if (val < INT_MIN || val > INT_MAX)
        return default_val;

    return (int)val;
}

//Helpers
void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data) {
    (void)user_data;
    fprintf(stderr, "PDF Error: error_no=%04X, detail_no=%d\n", (unsigned int)error_no, (int)detail_no);
}
//Remove or modify random number generator here
void generate_hex_code(char *hex, int length) {
    if (!hex || length <= 0) return;
    
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 0; i < length && i < HEX_LENGTH; ++i) {
        hex[i] = hex_chars[rand() % 16];
    }
    hex[length] = '\0';
}

// CSV parsing
void free_csv_data(CSVData *csv) {
    if (!csv) return;
    
    for (int i = 0; i < csv->row_count; i++) {
        if (csv->rows[i].fields) {
            for (int j = 0; j < csv->rows[i].count; j++) {
                free(csv->rows[i].fields[j]);
            }
            free(csv->rows[i].fields);
        }
    }
    
    if (csv->field_names) {
        for (int i = 0; i < csv->field_count; i++) {
            free(csv->field_names[i]);
        }
        free(csv->field_names);
    }
    
    free(csv->rows);
    free(csv);
}

CSVData* parse_csv(const char *filename) {
    if (!filename) {
        fprintf(stderr, "NULL filename provided\n");
        return NULL;
    }
    
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open CSV file: %s\n", filename);
        return NULL;
    }
    
    CSVData *csv = calloc(1, sizeof(CSVData));
    if (!csv) {
        fclose(f);
        return NULL;
    }
    
    char line[MAX_CSV_LINE_LEN];
    int capacity = 100;
    
    // Read header line
    if (!fgets(line, sizeof(line), f)) {
        free(csv);
        fclose(f);
        fprintf(stderr, "CSV file is empty\n");
        return NULL;
    }
    
    // Parse header with quoted field support
    csv->field_names = NULL;
    csv->field_count = 0;
    int field_capacity = 50;
    csv->field_names = malloc(field_capacity * sizeof(char*));
    if (!csv->field_names) {
        free(csv);
        fclose(f);
        return NULL;
    }
    
    char *ptr = line;
    while (*ptr && csv->field_count < MAX_CSV_FIELDS) {
        // Skip leading whitespace
        while (*ptr && isspace((unsigned char)*ptr)) ptr++;
        if (!*ptr) break;
        
        // Check for quoted field
        int quoted = 0;
        char *field_start = ptr;
        
        if (*ptr == '"') {
            quoted = 1;
            field_start = ++ptr; // Skip opening quote
        }
        
        // Find end of field
        while (*ptr) {
            if (quoted) {
                if (*ptr == '"') {
                    // Check if it's an escaped quote
                    if (*(ptr+1) == '"') {
                        ptr += 2; // Skip escaped quote
                        continue;
                    }
                    break; // Closing quote found
                }
            } else {
                if (*ptr == ',' || *ptr == '\n' || *ptr == '\r') break;
            }
            ptr++;
        }
        
        // Extract field
        char *end = ptr;
        if (quoted && *ptr == '"') {
            end = ptr; // Point to closing quote
            ptr++; // Move past closing quote
        }
        
        // Allocate and copy field
        int field_len = end - field_start;
        if (field_len < 0) {
            field_len = 0;
        }
        
        char *field = malloc(field_len + 1);
        if (!field) {
            fprintf(stderr, "Memory allocation error parsing CSV header\n");
            free_csv_data(csv);
            fclose(f);
            return NULL;
        }
        
        // Copy field content, handling escaped quotes
        char *dest = field;
        char *src = field_start;
        int copied = 0;
        while (src < end && copied < field_len) {
            if (quoted && *src == '"' && *(src+1) == '"') {
                *dest++ = '"';
                src += 2;
                copied++;
            } else {
                *dest++ = *src++;
                copied++;
            }
        }
        *dest = '\0';
        
        // Trim trailing whitespace from unquoted fields
        if (!quoted) {
            char *trim = field + strlen(field) - 1;
            while (trim > field && isspace((unsigned char)*trim)) {
                *trim-- = '\0';
            }
        }
        
        // Store field name
        if (csv->field_count >= field_capacity) {
            if (field_capacity >= MAX_CSV_FIELDS) break;
            field_capacity *= 2;
            if (field_capacity > MAX_CSV_FIELDS) field_capacity = MAX_CSV_FIELDS;
            
            char **new_fields = realloc(csv->field_names, field_capacity * sizeof(char*));
            if (!new_fields) {
                free(field);
                break;
            }
            csv->field_names = new_fields;
        }
        csv->field_names[csv->field_count++] = field;
        
        // Move to next field
        while (*ptr && (*ptr == ',' || isspace((unsigned char)*ptr))) ptr++;
    }
    
    // Parse data rows
    csv->rows = malloc(capacity * sizeof(CSVRow));
    if (!csv->rows) {
        free_csv_data(csv);
        fclose(f);
        return NULL;
    }
    csv->row_count = 0;
    
    while (fgets(line, sizeof(line), f) && csv->row_count < MAX_CSV_ROWS) {
        // Skip empty lines
        int empty = 1;
        for (char *p = line; *p; p++) {
            if (!isspace((unsigned char)*p) && *p != ',') {
                empty = 0;
                break;
            }
        }
        if (empty) continue;
        
        if (csv->row_count >= capacity) {
            if (capacity >= MAX_CSV_ROWS) break;
            capacity *= 2;
            if (capacity > MAX_CSV_ROWS) capacity = MAX_CSV_ROWS;
            
            CSVRow *new_rows = realloc(csv->rows, capacity * sizeof(CSVRow));
            if (!new_rows) break;
            csv->rows = new_rows;
        }
        
        CSVRow *row = &csv->rows[csv->row_count];
        row->fields = malloc(csv->field_count * sizeof(char*));
        if (!row->fields) {
            fprintf(stderr, "Memory allocation error for CSV row\n");
            break;
        }
        row->count = 0;
        
        ptr = line;
        int field_index = 0;
        
        while (*ptr && field_index < csv->field_count) {
            // Skip leading whitespace
            while (*ptr && isspace((unsigned char)*ptr)) ptr++;
            if (!*ptr) break;
            
            // Check for quoted field
            int quoted = 0;
            char *field_start = ptr;
            
            if (*ptr == '"') {
                quoted = 1;
                field_start = ++ptr; // Skip opening quote
            }
            
            // Find end of field
            while (*ptr) {
                if (quoted) {
                    if (*ptr == '"') {
                        // Check if it's an escaped quote
                        if (*(ptr+1) == '"') {
                            ptr += 2; // Skip escaped quote
                            continue;
                        }
                        break; // Closing quote found
                    }
                } else {
                    if (*ptr == ',' || *ptr == '\n' || *ptr == '\r') break;
                }
                ptr++;
            }
            
            // Extract field
            char *end = ptr;
            if (quoted && *ptr == '"') {
                end = ptr; // Point to closing quote
                ptr++; // Move past closing quote
            }
            
            // Allocate and copy field
            int field_len = end - field_start;
            if (field_len < 0) field_len = 0;
            
            char *field_val = malloc(field_len + 1);
            if (!field_val) {
                fprintf(stderr, "Memory allocation error parsing CSV data\n");
                // Clean up partial row
                for (int i = 0; i < row->count; i++) free(row->fields[i]);
                free(row->fields);
                break;
            }
            
            // Copy field content, handling escaped quotes
            char *dest = field_val;
            char *src = field_start;
            int copied = 0;
            while (src < end && copied < field_len) {
                if (quoted && *src == '"' && *(src+1) == '"') {
                    *dest++ = '"';
                    src += 2;
                    copied++;
                } else {
                    *dest++ = *src++;
                    copied++;
                }
            }
            *dest = '\0';
            
            // Trim trailing whitespace from unquoted fields
            if (!quoted) {
                char *trim = field_val + strlen(field_val) - 1;
                while (trim > field_val && isspace((unsigned char)*trim)) {
                    *trim-- = '\0';
                }
            }
            
            row->fields[field_index++] = field_val;
            row->count++;
            
            // Move to next field
            while (*ptr && (*ptr == ',' || isspace((unsigned char)*ptr))) ptr++;
        }
        
        // Fill missing fields with empty strings
        while (field_index < csv->field_count) {
            row->fields[field_index] = strdup("");
            if (!row->fields[field_index]) {
                break;
            }
            row->count++;
            field_index++;
        }
        
        csv->row_count++;
    }
    
    fclose(f);
    return csv;
}

// Drawing functions
void draw_qr_code(HPDF_Page page, float x, float y, float size, const char *text) {
    if (!page || !text || size <= 0) return;
    
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

    if (!qrcodegen_encodeText(text, tempBuffer, qrcode, qrcodegen_Ecc_MEDIUM,
                            qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                            qrcodegen_Mask_AUTO, true)) {
        return;
    }

    int qr_size = qrcodegen_getSize(qrcode);
    if (qr_size <= 0) return;
    
    float scale = size / qr_size;

    HPDF_Page_GSave(page);
    HPDF_Page_Concat(page, scale, 0, 0, scale, x, y);

    for (int iy = 0; iy < qr_size; iy++) {
        for (int ix = 0; ix < qr_size; ix++) {
            if (qrcodegen_getModule(qrcode, ix, iy)) {
                HPDF_Page_Rectangle(page, ix, qr_size - 1 - iy, 1, 1);
            }
        }
    }

    HPDF_Page_Fill(page);
    HPDF_Page_GRestore(page);
}

void draw_text_in_box(HPDF_Page page, HPDF_Font font,
                      float x_start, float x_end, float y_start, float y_end,
                      const char *text, float font_size, int align) {
    if (!page || !font || !text || font_size <= 0) return;
    if (x_end <= x_start || y_end <= y_start) return;

    const float padding = 5.0f;
    const float box_width = (x_end - x_start) - 2 * padding;
    const float box_height = (y_end - y_start) - 2 * padding;
    if (box_width <= 0 || box_height <= 0) return;

    char *buf = strdup(text);
    if (!buf) return;

    // Split words
    char *words[1024];
    int wc = 0;
    char *tok, *saveptr = NULL;
    for (tok = strtok_r(buf, " ", &saveptr);
         tok && wc < 1023;
         tok = strtok_r(NULL, " ", &saveptr))
        words[wc++] = tok;
    words[wc] = NULL;


    float test_size = font_size;
    int fits = 0;// lines_used = 0;

    while (!fits && test_size >= 6.0f) {
        HPDF_Page_SetFontAndSize(page, font, test_size);
        float line_height = test_size * 1.2f;
        int lines = 1;
        float line_width = 0.0f;

        for (int i = 0; i < wc; i++) {
            float word_width = HPDF_Page_TextWidth(page, words[i]);
            float space_width = HPDF_Page_TextWidth(page, " ");

            if (line_width == 0)
                line_width = word_width;
            else if (line_width + space_width + word_width > box_width) {
                lines++;
                line_width = word_width;
            } else {
                line_width += space_width + word_width;
            }
        }

        if (lines * line_height <= box_height) {
            fits = 1;
            //lines_used = lines;
        } else {
            test_size -= 1.0f;
        }
    }

    HPDF_Page_SetFontAndSize(page, font, test_size);
    const float line_height = test_size * 1.2f;


    HPDF_Page_BeginText(page);

    //float total_height = lines_used * line_height;
    float y_cursor = y_end - padding - test_size; // top-down baseline

    char line[2048] = "";
    for (int i = 0; i < wc; i++) {
        char candidate[2048];
        snprintf(candidate, sizeof(candidate), "%s%s%s",
                 line, (strlen(line) > 0 ? " " : ""), words[i]);

        float candidate_width = HPDF_Page_TextWidth(page, candidate);

        if (candidate_width > box_width && strlen(line) > 0) {
            // Draw current line
            float lw = HPDF_Page_TextWidth(page, line);
            float x_offset = x_start + padding;

            if (align == 1) // center
                x_offset = x_start + (box_width - lw) / 2.0f + padding;
            else if (align == 2) // right
                x_offset = x_end - lw - padding;

            HPDF_Page_TextOut(page, x_offset, y_cursor, line);

            y_cursor -= line_height;
            if (y_cursor < y_start + padding) break;

            snprintf(line, sizeof(line), "%s", words[i]);
        } else {
            snprintf(line, sizeof(line), "%s", candidate);
        }
    }

    // Draw last line if any
    if (strlen(line) > 0 && y_cursor >= y_start + padding) {
        float lw = HPDF_Page_TextWidth(page, line);
        float x_offset = x_start + padding;

        if (align == 1)
            x_offset = x_start + (box_width - lw) / 2.0f + padding;
        else if (align == 2)
            x_offset = x_end - lw - padding;

        HPDF_Page_TextOut(page, x_offset, y_cursor, line);
    }

    HPDF_Page_EndText(page);
    free(buf);
}


// JSON loading
int parse_align(const char *s) {
    if (!s) return 0;
    if (strcmp(s, "center") == 0) return 1;
    if (strcmp(s, "right") == 0) return 2;
    return 0;
}

HPDF_PageSizes parse_page_size(const char *s) {
    if (!s) return HPDF_PAGE_SIZE_A4;
    if (strcmp(s, "A3") == 0) return HPDF_PAGE_SIZE_A3;
    if (strcmp(s, "A4") == 0) return HPDF_PAGE_SIZE_A4;
    if (strcmp(s, "A5") == 0) return HPDF_PAGE_SIZE_A5;
    if (strcmp(s, "LETTER") == 0) return HPDF_PAGE_SIZE_LETTER;
    if (strcmp(s, "LEGAL") == 0) return HPDF_PAGE_SIZE_LEGAL;
    return HPDF_PAGE_SIZE_A4; // Default
}

HPDF_PageDirection parse_orientation(const char *s) {
    if (!s) return HPDF_PAGE_PORTRAIT;
    if (strcmp(s, "landscape") == 0) return HPDF_PAGE_LANDSCAPE;
    return HPDF_PAGE_PORTRAIT; // Default
}

int load_page_config_from_json(cJSON *root, PageConfig *config) {
    if (!root || !config) return -1;

    cJSON *jpage = cJSON_GetObjectItem(root, "page");
    if (!jpage) return -1;

    const char *size_str = cJSON_GetObjectItem(jpage, "size") && cJSON_GetObjectItem(jpage, "size")->valuestring
                         ? cJSON_GetObjectItem(jpage, "size")->valuestring : "A4";
    config->size = parse_page_size(size_str);

    const char *orient_str = cJSON_GetObjectItem(jpage, "orientation") && cJSON_GetObjectItem(jpage, "orientation")->valuestring
                           ? cJSON_GetObjectItem(jpage, "orientation")->valuestring : "portrait";
    config->orientation = parse_orientation(orient_str);

    config->line_width = (float)(cJSON_GetObjectItem(jpage, "line_width") ? cJSON_GetObjectItem(jpage, "line_width")->valuedouble : 3.0f);

    if (config->line_width <= 0.0f) {
        fprintf(stderr, "Warning: line_width must be positive, using default 3.0\n");
        config->line_width = 3.0f;
    }

    return 0;
}

int load_fonts_from_json(cJSON *root, FontConfig *font_config, HPDF_Doc pdf) {
    if (!root || !font_config || !pdf) return -1;
    
    cJSON *jfonts = cJSON_GetObjectItem(root, "fonts");
    if (!jfonts) {
        safe_strncpy(font_config->default_font, "Helvetica-Bold", sizeof(font_config->default_font));
        font_config->custom_fonts = NULL;
        font_config->custom_font_count = 0;
        return 0;
    }
    
    cJSON *jdefault = cJSON_GetObjectItem(jfonts, "default");
    if (jdefault && jdefault->valuestring) {
        safe_strncpy(font_config->default_font, jdefault->valuestring, sizeof(font_config->default_font));
    } else {
        safe_strncpy(font_config->default_font, "Helvetica-Bold", sizeof(font_config->default_font));
    }
    
    cJSON *jcustom = cJSON_GetObjectItem(jfonts, "custom_fonts");
    if (jcustom && cJSON_IsArray(jcustom)) {
        int count = cJSON_GetArraySize(jcustom);
        if (count > MAX_CUSTOM_FONTS) {
            fprintf(stderr, "Warning: Too many custom fonts (%d), limiting to %d\n", count, MAX_CUSTOM_FONTS);
            count = MAX_CUSTOM_FONTS;
        }
        
        font_config->custom_fonts = malloc(sizeof(CustomFont) * count);
        if (!font_config->custom_fonts) return -1;
        
        font_config->custom_font_count = 0;
        
        for (int i = 0; i < count; i++) {
            cJSON *jfont = cJSON_GetArrayItem(jcustom, i);
            if (!jfont) continue;
            
            cJSON *jname = cJSON_GetObjectItem(jfont, "name");
            cJSON *jfile = cJSON_GetObjectItem(jfont, "file");
            cJSON *jencoding = cJSON_GetObjectItem(jfont, "encoding");
            
            if (jname && jname->valuestring && jfile && jfile->valuestring) {
                CustomFont *font = &font_config->custom_fonts[font_config->custom_font_count];
                
                safe_strncpy(font->name, jname->valuestring, sizeof(font->name));
                safe_strncpy(font->file, jfile->valuestring, sizeof(font->file));
                
                if (jencoding && jencoding->valuestring) {
                    safe_strncpy(font->encoding, jencoding->valuestring, sizeof(font->encoding));
                } else {
                    safe_strncpy(font->encoding, "WinAnsiEncoding", sizeof(font->encoding));
                }
                
                const char *loaded_font_name = HPDF_LoadTTFontFromFile(pdf, font->file, HPDF_TRUE);
                if (!loaded_font_name) {
                    fprintf(stderr, "Warning: Could not load font file: %s\n", font->file);
                } else {
                    printf("Loaded font: %s from %s\n", font->name, font->file);
                }
                
                font_config->custom_font_count++;
            }
        }
    }
    
    return 0;
}

int load_fields_from_json(cJSON *root, Field **out_fields, int *out_count, 
                         const char *hex_code, CSVData *csv, int csv_row_index)
{
    if (!root || !out_fields || !out_count || !hex_code) return -1;

    cJSON *jfields = cJSON_GetObjectItem(root, "fields");
    if (!jfields || !cJSON_IsArray(jfields)) return -1;

    int count = cJSON_GetArraySize(jfields);
    if (count > MAX_FIELD_COUNT) {
        fprintf(stderr, "Warning: Too many fields (%d), limiting to %d\n", count, MAX_FIELD_COUNT);
        count = MAX_FIELD_COUNT;
    }
    
    Field *arr = (Field*)calloc(count, sizeof(Field));
    if (!arr) return -2;

    int valid_count = 0;

    for (int i = 0; i < count; ++i) {
        cJSON *it = cJSON_GetArrayItem(jfields, i);
        if (!it || !cJSON_IsObject(it))
            continue;

        Field tmp;
        memset(&tmp, 0, sizeof(tmp));

        // Required numeric fields
        cJSON *x_start    = cJSON_GetObjectItem(it, "x_start");
        cJSON *x_end      = cJSON_GetObjectItem(it, "x_end");
        cJSON *y_start    = cJSON_GetObjectItem(it, "y_start");
        cJSON *y_end      = cJSON_GetObjectItem(it, "y_end");
        cJSON *font_size  = cJSON_GetObjectItem(it, "font_size");

        if (!cJSON_IsNumber(x_start) ||
            !cJSON_IsNumber(x_end)   ||
            !cJSON_IsNumber(y_start) ||
            !cJSON_IsNumber(y_end)   ||
            !cJSON_IsNumber(font_size))
        {
            fprintf(stderr, "Warning: Field %d: missing/wrong type in required numeric field. Skipping.\n", i);
            continue;
        }

        tmp.x_start   = (float)x_start->valuedouble;
        tmp.x_end     = (float)x_end->valuedouble;
        tmp.y_start   = (float)y_start->valuedouble;
        tmp.y_end     = (float)y_end->valuedouble;
        tmp.font_size = (float)font_size->valuedouble;


        cJSON *jwrap = cJSON_GetObjectItem(it, "wrap");
        if (cJSON_IsBool(jwrap))
            tmp.wrap = cJSON_IsTrue(jwrap);
        else if (cJSON_IsNumber(jwrap))
            tmp.wrap = jwrap->valueint != 0;
        else
            tmp.wrap = 0;


        cJSON *jalign = cJSON_GetObjectItem(it, "align");
        const char *align_s = (cJSON_IsString(jalign) ? jalign->valuestring : "left");
        tmp.align = parse_align(align_s);

        // font name
        cJSON *jfont = cJSON_GetObjectItem(it, "font_name");
        if (cJSON_IsString(jfont))
            safe_strncpy(tmp.font_name, jfont->valuestring, sizeof(tmp.font_name));
        else
            tmp.font_name[0] = '\0';

        //max length field truncate function
        cJSON *jmax_len = cJSON_GetObjectItem(it, "max_length");
        if (cJSON_IsNumber(jmax_len)) {
            tmp.max_length = jmax_len->valueint;
            if (tmp.max_length < 0) tmp.max_length = 0;
            if (tmp.max_length > MAX_FIELD_LEN) tmp.max_length = MAX_FIELD_LEN;
        } else {
            tmp.max_length = 0;
        }


        cJSON *jtext = cJSON_GetObjectItem(it, "text");
        const char *txt = (cJSON_IsString(jtext) ? jtext->valuestring : "");

        // set CSV field substitution character
        if (txt[0] == '$' && csv && csv_row_index >= 0 && csv_row_index < csv->row_count) {

            char field_name[256];
            safe_strncpy(field_name, txt + 1, sizeof(field_name));

            int found = 0;
            for (int c = 0; c < csv->field_count; c++) {
                if (strcmp(csv->field_names[c], field_name) == 0) {
                    const char *val = csv->rows[csv_row_index].fields[c];

                    size_t len = strlen(val);
                    if (tmp.max_length > 0 && len > (size_t)tmp.max_length) {
                        safe_strncpy(tmp.text, val, tmp.max_length + 1);
                        printf("Notice: Truncated field '%s'\n", field_name);
                    } else {
                        safe_strncpy(tmp.text, val, sizeof(tmp.text));
                    }
                    found = 1;
                    break;
                }
            }
            if (!found)
                safe_strncpy(tmp.text, txt, sizeof(tmp.text));
        }
        else if (!strcmp(txt, "HEX_CODE") || !strcmp(txt, "RANDOM_HEX")) {
            safe_strncpy(tmp.text, hex_code, sizeof(tmp.text));
        }
        else {
            safe_strncpy(tmp.text, txt, sizeof(tmp.text));
        }

        // Add validated item to list
        arr[valid_count++] = tmp;
    }

    *out_fields = arr;
    *out_count = valid_count;
    return 0;
}

int load_lines_from_json(cJSON *root, LineEntry **out_lines, int *out_count) {
    if (!root || !out_lines || !out_count) return -1;
    
    cJSON *jlines = cJSON_GetObjectItem(root, "lines");
    if (!jlines || !cJSON_IsArray(jlines)) {
        *out_lines = NULL;
        *out_count = 0;
        return 0;
    }
    
    int count = cJSON_GetArraySize(jlines);
    if (count > MAX_LINE_COUNT) {
        fprintf(stderr, "Warning: Too many lines (%d), limiting to %d\n", count, MAX_LINE_COUNT);
        count = MAX_LINE_COUNT;
    }
    
    LineEntry *arr = (LineEntry*)calloc(count, sizeof(LineEntry));
    if (!arr) return -1;

    for (int i = 0; i < count; ++i) {
        cJSON *it = cJSON_GetArrayItem(jlines, i);
        if (!it) continue;

        // Initialize with default width of 1.0 (or use page_config.line_width as default)
        arr[i].width = 1.0f;  // Default line width
        
        cJSON *type = cJSON_GetObjectItem(it, "type");
        if (type && type->valuestring && strcmp(type->valuestring, "horizontal_transform") == 0) {
            arr[i].type = LINE_H_TRANSFORM;
            cJSON *jy = cJSON_GetObjectItem(it, "y");
            if (!jy) {
                fprintf(stderr, "Warning: Missing 'y' for horizontal_transform line, using default\n");
                arr[i].y = 0.0f;
            } else {
                arr[i].y = (float)jy->valuedouble;
            }
            arr[i].x_start = (float)(cJSON_GetObjectItem(it, "x_start") ? cJSON_GetObjectItem(it, "x_start")->valuedouble : 0.0f);
            arr[i].x_end = (float)(cJSON_GetObjectItem(it, "x_end") ? cJSON_GetObjectItem(it, "x_end")->valuedouble : 841.89f);
            arr[i].y_start = arr[i].y_end = arr[i].y;
        } else {
            arr[i].type = LINE_RAW;
            cJSON *jx_start = cJSON_GetObjectItem(it, "x_start");
            cJSON *jy_start = cJSON_GetObjectItem(it, "y_start");
            cJSON *jx_end = cJSON_GetObjectItem(it, "x_end");
            cJSON *jy_end = cJSON_GetObjectItem(it, "y_end");
            
            arr[i].x_start = jx_start ? (float)jx_start->valuedouble : 0.0f;
            arr[i].y_start = jy_start ? (float)jy_start->valuedouble : 0.0f;
            arr[i].x_end = jx_end ? (float)jx_end->valuedouble : 0.0f;
            arr[i].y_end = jy_end ? (float)jy_end->valuedouble : 0.0f;
        }
        
        // Load individual line width if specified
        cJSON *jwidth = cJSON_GetObjectItem(it, "width");
        if (jwidth && cJSON_IsNumber(jwidth)) {
            arr[i].width = (float)jwidth->valuedouble;
            if (arr[i].width <= 0.0f) {
                fprintf(stderr, "Warning: Line %d width must be positive, using 1.0\n", i);
                arr[i].width = 1.0f;
            }
        }
    }
    *out_lines = arr;
    *out_count = count;
    return 0;
}



int load_qr_from_json(cJSON *root, QRCodeEntry *qr_entry, const char *hex_code, CSVData *csv, int csv_row_index) {
    if (!root || !qr_entry) return -1;
    
    cJSON *jqr = cJSON_GetObjectItem(root, "qr_code");
    if (!jqr) {
        // No QR code configuration found, disable it
        qr_entry->enabled = 0;
        return 0;
    }
    
    // Set defaults
    qr_entry->x = 192.0f;
    qr_entry->y = 1.0f;
    qr_entry->size = 113.4f;
    qr_entry->text[0] = '\0';
    qr_entry->enabled = 1;  // Enable by default if config exists
    
    // Safely get values with null checks
    cJSON *jx = cJSON_GetObjectItem(jqr, "x");
    cJSON *jy = cJSON_GetObjectItem(jqr, "y");
    cJSON *jsize = cJSON_GetObjectItem(jqr, "size");
    cJSON *jenabled = cJSON_GetObjectItem(jqr, "enabled");
    
    if (jx) qr_entry->x = (float)jx->valuedouble;
    if (jy) qr_entry->y = (float)jy->valuedouble;
    if (jsize) qr_entry->size = (float)jsize->valuedouble;
    if (jenabled) qr_entry->enabled = cJSON_IsTrue(jenabled) ? 1 : 0;
    
    const char *t = cJSON_GetObjectItem(jqr, "text") && cJSON_GetObjectItem(jqr, "text")->valuestring
                    ? cJSON_GetObjectItem(jqr, "text")->valuestring : "";
    
    // Process text (same logic as fields and barcodes)
    if (t[0] == '$' && csv && csv_row_index >= 0 && csv_row_index < csv->row_count) {
        char field_name[256];
        safe_strncpy(field_name, t + 1, sizeof(field_name));
        
        int found = 0;
        for (int j = 0; j < csv->field_count; j++) {
            if (strcmp(csv->field_names[j], field_name) == 0) {
                if (j < csv->rows[csv_row_index].count) {
                    safe_strncpy(qr_entry->text, csv->rows[csv_row_index].fields[j], sizeof(qr_entry->text));
                    found = 1;
                }
                break;
            }
        }
        if (!found) {
            safe_strncpy(qr_entry->text, t, sizeof(qr_entry->text));
        }
    }
    else if (strcmp(t, "HEX_CODE") == 0 || strcmp(t, "RANDOM_HEX") == 0) {
        safe_strncpy(qr_entry->text, hex_code, sizeof(qr_entry->text));
    } else {
        safe_strncpy(qr_entry->text, t, sizeof(qr_entry->text));
    }
    
    return 0;
}


int load_barcodes_from_json(cJSON *root, BarcodeEntry **out_barcodes, int *out_count, 
                           const char *hex_code, CSVData *csv, int csv_row_index) {
    if (!root || !out_barcodes || !out_count || !hex_code) return -1;

    cJSON *jbarcodes = cJSON_GetObjectItem(root, "barcodes");
    if (!jbarcodes || !cJSON_IsArray(jbarcodes)) {
        *out_barcodes = NULL;
        *out_count = 0;
        return 0;
    }

    int count = cJSON_GetArraySize(jbarcodes);
    if (count > MAX_FIELD_COUNT) {
        fprintf(stderr, "Warning: Too many barcodes (%d), limiting to %d\n", count, MAX_FIELD_COUNT);
        count = MAX_FIELD_COUNT;
    }
    
    BarcodeEntry *arr = (BarcodeEntry*)calloc(count, sizeof(BarcodeEntry));
    if (!arr) return -2;

    for (int i = 0; i < count; ++i) {
        cJSON *it = cJSON_GetArrayItem(jbarcodes, i);
        if (!it) continue;

        // Get required fields
        cJSON *jx = cJSON_GetObjectItem(it, "x");
        cJSON *jy = cJSON_GetObjectItem(it, "y");
        cJSON *jwidth = cJSON_GetObjectItem(it, "width");
        cJSON *jheight = cJSON_GetObjectItem(it, "height");
        cJSON *jtype = cJSON_GetObjectItem(it, "type");
        
        if (!jx || !jy || !jwidth || !jheight || !jtype) {
            fprintf(stderr, "Warning: Missing required barcode field in barcode %d, skipping\n", i);
            continue;
        }

        arr[i].x = (float)jx->valuedouble;
        arr[i].y = (float)jy->valuedouble;
        arr[i].width = (float)jwidth->valuedouble;
        arr[i].height = (float)jheight->valuedouble;
        
        safe_strncpy(arr[i].type, jtype->valuestring, sizeof(arr[i].type));

        const char *txt = cJSON_GetObjectItem(it, "text") && cJSON_GetObjectItem(it, "text")->valuestring
                          ? cJSON_GetObjectItem(it, "text")->valuestring : "";
        
        // Process text (same logic as fields)
        if (txt[0] == '$' && csv && csv_row_index >= 0 && csv_row_index < csv->row_count) {
            char field_name[256];
            safe_strncpy(field_name, txt + 1, sizeof(field_name));
            
            int found = 0;
            for (int j = 0; j < csv->field_count; j++) {
                if (strcmp(csv->field_names[j], field_name) == 0) {
                    if (j < csv->rows[csv_row_index].count) {
                        safe_strncpy(arr[i].text, csv->rows[csv_row_index].fields[j], sizeof(arr[i].text));
                        found = 1;
                    }
                    break;
                }
            }
            if (!found) {
                safe_strncpy(arr[i].text, txt, sizeof(arr[i].text));
            }
        }
        else if (strcmp(txt, "HEX_CODE") == 0 || strcmp(txt, "RANDOM_HEX") == 0) {
            safe_strncpy(arr[i].text, hex_code, sizeof(arr[i].text));
        } else {
            safe_strncpy(arr[i].text, txt, sizeof(arr[i].text));
        }
    }

    *out_barcodes = arr;
    *out_count = count;
    return 0;
}

void draw_barcode_entry(HPDF_Page page, BarcodeEntry *barcode) {
    if (!page || !barcode) return;
    
    BarcodeType type;
    if (strcmp(barcode->type, "code128") == 0) {
        type = BARCODE_CODE128;
    } else if (strcmp(barcode->type, "ean13") == 0) {
        type = BARCODE_EAN13;
    } else if (strcmp(barcode->type, "upca") == 0) {
        type = BARCODE_UPCA;
    } else {
        fprintf(stderr, "Warning: Unknown barcode type: %s\n", barcode->type);
        return;
    }
    
    // Validate barcode data before drawing
    if (!validate_barcode_data(type, barcode->text)) {
        fprintf(stderr, "Warning: Invalid barcode data for type %s: %s\n", barcode->type, barcode->text);
        return;
    }
    
    draw_barcode(page, barcode->x, barcode->y, barcode->width, barcode->height, type, barcode->text);
}

int validate_json_config(cJSON *root) {
    if (!root) return -1;
    
    cJSON *jpage = cJSON_GetObjectItem(root, "page");
    if (!jpage) {
        fprintf(stderr, "Error: Missing 'page' section in config\n");
        return -1;
    }
    
    cJSON *jfields = cJSON_GetObjectItem(root, "fields");
    if (!jfields || !cJSON_IsArray(jfields)) {
        fprintf(stderr, "Error: Missing or invalid 'fields' array in config\n");
        return -1;
    }
    
    return 0;
}

// Command Line Help
void print_version() {
    printf("FDCLabel - Fast Dynamic C Label Generator\n");
    printf("Version 0.8.0\n");
    printf("Copyright (C) Ivan Rolero\n");
    printf("License: GNU GPL v3\n");
}

void print_help(const char *program_name) {
    printf("Usage: %s <csv_file> [options]\n", program_name);
    printf("\nRequired:\n");
    printf("  csv_file              Path to CSV data file\n");
    printf("\nOptions:\n");
    printf("  -c, --config FILE     JSON configuration file (default: config.json)\n");
    printf("  -o, --output FILE     Output PDF filename (default: labels.pdf)\n");
    printf("  -r, --row INDEX       Process specific row only (default: all rows)\n");
    printf("  --validate            Validate configuration without generating PDF\n");
    printf("  -v, --version         Show version information\n");
    printf("  -h, --help            Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s data.csv                     # Use defaults\n", program_name);
    printf("  %s data.csv -c config1.json     # Custom config\n", program_name);
    printf("  %s data.csv -o output.pdf -r 5  # Specific output and row\n", program_name);
    printf("  %s data.csv --validate          # Validate config only\n", program_name);
}

/* ---------- Configuration Validation ---------- */
int validate_config_only(const char *config_filename) {
    printf("Validating configuration: %s\n", config_filename);
    
    FILE *f = fopen(config_filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open config file: %s\n", config_filename);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size > MAX_CONFIG_SIZE) {
        fprintf(stderr, "Error: Config file too large: %ld bytes (max: %d)\n", size, MAX_CONFIG_SIZE);
        fclose(f);
        return 1;
    }
    rewind(f);
    
    char *data = (char*)malloc(size + 1);
    if (!data) {
        fclose(f);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    
    size_t read_size = fread(data, 1, size, f);
    if (read_size != (size_t)size) {
        fprintf(stderr, "Error reading config file\n");
        free(data);
        fclose(f);
        return 1;
    }
    
    data[size] = '\0';
    fclose(f);
    
    cJSON *root = cJSON_Parse(data);
    free(data);
    
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            fprintf(stderr, "Error: JSON parsing failed before: %s\n", error_ptr);
        }
        return 1;
    }
    
    if (validate_json_config(root) != 0) {
        fprintf(stderr, "Error: Invalid configuration structure\n");
        cJSON_Delete(root);
        return 1;
    }
    
    // Additional validation checks
    cJSON *jpage = cJSON_GetObjectItem(root, "page");
    if (jpage) {
        cJSON *jsize = cJSON_GetObjectItem(jpage, "size");
        if (jsize && jsize->valuestring) {
            const char *valid_sizes[] = {"A3", "A4", "A5", "LETTER", "LEGAL", NULL};
            int valid = 0;
            for (int i = 0; valid_sizes[i]; i++) {
                if (strcmp(jsize->valuestring, valid_sizes[i]) == 0) {
                    valid = 1;
                    break;
                }
            }
            if (!valid) {
                fprintf(stderr, "Warning: Unknown page size: %s\n", jsize->valuestring);
            }
        }
        
        cJSON *jorientation = cJSON_GetObjectItem(jpage, "orientation");
        if (jorientation && jorientation->valuestring) {
            if (strcmp(jorientation->valuestring, "portrait") != 0 && 
                strcmp(jorientation->valuestring, "landscape") != 0) {
                fprintf(stderr, "Warning: Unknown orientation: %s (use 'portrait' or 'landscape')\n", 
                        jorientation->valuestring);
            }
        }
    }
    
    // Validate fonts exist if specified
    cJSON *jfonts = cJSON_GetObjectItem(root, "fonts");
    if (jfonts) {
        cJSON *jcustom = cJSON_GetObjectItem(jfonts, "custom_fonts");
        if (jcustom && cJSON_IsArray(jcustom)) {
            int count = cJSON_GetArraySize(jcustom);
            for (int i = 0; i < count; i++) {
                cJSON *jfont = cJSON_GetArrayItem(jcustom, i);
                cJSON *jfile = cJSON_GetObjectItem(jfont, "file");
                if (jfile && jfile->valuestring) {
                    FILE *test = fopen(jfile->valuestring, "rb");
                    if (!test) {
                        fprintf(stderr, "Warning: Font file not found: %s\n", jfile->valuestring);
                    } else {
                        fclose(test);
                        printf("Font file OK: %s\n", jfile->valuestring);
                    }
                }
            }
        }
    }
    
    // Validate fields structure
    cJSON *jfields = cJSON_GetObjectItem(root, "fields");
    if (jfields && cJSON_IsArray(jfields)) {
        int count = cJSON_GetArraySize(jfields);
        printf("Found %d field definitions\n", count);
        
        for (int i = 0; i < count; i++) {
            cJSON *jfield = cJSON_GetArrayItem(jfields, i);
            if (!cJSON_GetObjectItem(jfield, "x_start") || 
                !cJSON_GetObjectItem(jfield, "x_end") ||
                !cJSON_GetObjectItem(jfield, "y_start") || 
                !cJSON_GetObjectItem(jfield, "y_end")) {
                fprintf(stderr, "Warning: Field %d missing required coordinates\n", i);
            }
        }
    }
    
    printf("? Configuration is valid: %s\n", config_filename);
    cJSON_Delete(root);
    return 0;

}
