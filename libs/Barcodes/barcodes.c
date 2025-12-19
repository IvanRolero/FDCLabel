#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "barcodes.h"

/* ---------- Code128 Implementation ---------- */

// Code128 character encodings (patterns for A, B, C sets)
static const char* code128_encoding[107] = {
    "11011001100", "11001101100", "11001100110", "10010011000", "10010001100",
    "10001001100", "10011001000", "10011000100", "10001100100", "11001001000",
    "11001000100", "11000100100", "10110011100", "10011011100", "10011001110",
    "10111001100", "10011101100", "10011100110", "11001110010", "11001011100",
    "11001001110", "11011100100", "11001110100", "11101101110", "11101001100",
    "11100101100", "11100100110", "11101100100", "11100110100", "11100110010",
    "11011011000", "11011000110", "11000110110", "10100011000", "10001011000",
    "10001000110", "10110001000", "10001101000", "10001100010", "11010001000",
    "11000101000", "11000100010", "10110111000", "10110001110", "10001101110",
    "10111011000", "10111000110", "10001110110", "11101110110", "11010001110",
    "11000101110", "11011101000", "11011100010", "11011101110", "11101011000",
    "11101000110", "11100010110", "11101101000", "11101100010", "11100011010",
    "11101111010", "11001000010", "11110001010", "10100110000", "10100001100",
    "10010110000", "10010000110", "10000101100", "10000100110", "10110010000",
    "10110000100", "10011010000", "10011000010", "10000110100", "10000110010",
    "11000010010", "11001010000", "11110111010", "11000010100", "10001111010",
    "10100111100", "10010111100", "10010011110", "10111100100", "10011110100",
    "10011110010", "11110100100", "11110010100", "11110010010", "11011011110",
    "11011110110", "11110110110", "10101111000", "10100011110", "10001011110",
    "10111101000", "10111100010", "11110101000", "11110100010", "10111011110",
    "10111101110", "11101011110", "11110101110", "11010000100", "11010010000",
    "11010011100", "11000111010"
};

// Code128 start codes
#define CODE128_START_A 103
#define CODE128_START_B 104
#define CODE128_START_C 105

// Code128 stop code
#define CODE128_STOP 106

// Calculate Code128 checksum
static int code128_checksum(const int* codes, int length) {
    int sum = codes[0];
    for (int i = 1; i < length; i++) {
        sum += i * codes[i];
    }
    return sum % 103;
}

// Determine best Code128 encoding and convert string to codes
static int code128_encode_string(const char* text, int* codes) {
    if (!text || !codes) return 0;
    
    int len = strlen(text);
    if (len == 0) return 0;
    
    // For simplicity, we'll use Set B (most common)
    codes[0] = CODE128_START_B;
    int code_count = 1;
    
    for (int i = 0; i < len; i++) {
        unsigned char c = text[i];
        if (c >= 32 && c <= 126) {
            codes[code_count] = c - 32;
            code_count++;
        } else {
            // For simplicity, replace unsupported chars with space
            codes[code_count] = 0;
            code_count++;
        }
    }
    
    // Add checksum - fix the sequence point issue
    int checksum = code128_checksum(codes, code_count);
    codes[code_count] = checksum;
    code_count++;
    
    // Add stop code
    codes[code_count] = CODE128_STOP;
    code_count++;
    
    return code_count;
}

/* ---------- EAN/UPC Implementation ---------- */

// EAN-13 left-hand patterns (A and B encodings)
static const char* ean_left_patterns[10][2] = {
    {"0001101", "0100111"}, {"0011001", "0110011"}, {"0010011", "0011011"},
    {"0111101", "0100001"}, {"0100011", "0011101"}, {"0110001", "0111001"},
    {"0101111", "0000101"}, {"0111011", "0010001"}, {"0110111", "0001001"},
    {"0001011", "0010111"}
};

// EAN-13 right-hand patterns
static const char* ean_right_patterns[10] = {
    "1110010", "1100110", "1101100", "1000010", "1011100",
    "1001110", "1010000", "1000100", "1001000", "1110100"
};

// EAN-13 first digit encoding (determines left-hand pattern set)
static const char* ean_first_digit_patterns[10] = {
    "AAAAAA", "AABABB", "AABBAB", "AABBBA", "ABAABB",
    "ABBAAB", "ABBBAA", "ABABAB", "ABABBA", "ABBABA"
};

// Verify EAN-13 checksum
static int ean13_verify_checksum(const char* ean) {
    if (strlen(ean) != 13) return 0;
    
    int sum = 0;
    for (int i = 0; i < 12; i++) {
        int digit = ean[i] - '0';
        sum += (i % 2 == 0) ? digit : digit * 3;
    }
    
    int checksum = (10 - (sum % 10)) % 10;
    return checksum == (ean[12] - '0');
}

// Verify UPC-A checksum
static int upca_verify_checksum(const char* upc) {
    if (strlen(upc) != 12) return 0;
    
    int sum = 0;
    for (int i = 0; i < 11; i++) {
        int digit = upc[i] - '0';
        sum += (i % 2 == 0) ? digit * 3 : digit;
    }
    
    int checksum = (10 - (sum % 10)) % 10;
    return checksum == (upc[11] - '0');
}

/* ---------- Public Barcode Functions ---------- */

int validate_barcode_data(BarcodeType type, const char* data) {
    if (!data) return 0;
    
    switch (type) {
        case BARCODE_CODE128:
            // Code128 can encode most ASCII characters
            return strlen(data) > 0;
            
        case BARCODE_EAN13:
            if (strlen(data) != 13) return 0;
            for (int i = 0; i < 13; i++) {
                if (data[i] < '0' || data[i] > '9') return 0;
            }
            return ean13_verify_checksum(data);
            
        case BARCODE_UPCA:
            if (strlen(data) != 12) return 0;
            for (int i = 0; i < 12; i++) {
                if (data[i] < '0' || data[i] > '9') return 0;
            }
            return upca_verify_checksum(data);
            
        default:
            return 0;
    }
}

void draw_barcode(HPDF_Page page, float x, float y, float width, float height, 
                  BarcodeType type, const char* data) {
    if (!page || !data || width <= 0 || height <= 0) return;
    
    HPDF_Page_GSave(page);
    
    switch (type) {
        case BARCODE_CODE128:
            draw_code128(page, x, y, width, height, data);
            break;
            
        case BARCODE_EAN13:
            draw_ean13(page, x, y, width, height, data);
            break;
            
        case BARCODE_UPCA:
            draw_upca(page, x, y, width, height, data);
            break;
            
        default:
            break;
    }
    
    HPDF_Page_GRestore(page);
}

void draw_code128(HPDF_Page page, float x, float y, float width, float height, const char* data) {
    if (!validate_barcode_data(BARCODE_CODE128, data)) return;
    
    int codes[256];
    int code_count = code128_encode_string(data, codes);
    if (code_count == 0) return;
    
    // Calculate module width
    int total_modules = 0;
    for (int i = 0; i < code_count; i++) {
        total_modules += strlen(code128_encoding[codes[i]]);
    }
    total_modules += 11; // Add quiet zones
    
    float module_width = width / total_modules;
    float current_x = x;
    
    // Left quiet zone (10 modules)
    current_x += module_width * 10;
    
    // Group adjacent bars into single rectangles
    for (int i = 0; i < code_count; i++) {
        const char* pattern = code128_encoding[codes[i]];
        int pattern_len = strlen(pattern);
        
        int j = 0;
        while (j < pattern_len) {
            if (pattern[j] == '1') {
                // Find consecutive '1's to draw as a single rectangle
                int start = j;
                while (j < pattern_len && pattern[j] == '1') {
                    j++;
                }
                int consecutive_ones = j - start;
                
                HPDF_Page_Rectangle(page,
                    current_x + (start * module_width),
                    y,
                    consecutive_ones * module_width,
                    height
                );
                HPDF_Page_Fill(page);
            } else {
                j++;
            }
        }
        current_x += pattern_len * module_width;
    }
}

void draw_ean13(HPDF_Page page, float x, float y, float width, float height, const char* data) {
    if (!validate_barcode_data(BARCODE_EAN13, data)) return;
    
    // Calculate module width
    float module_width = width / 95.0f; // EAN-13 has 95 modules total
    float current_x = x;
    
    // Left guard bars - draw as single rectangles
    HPDF_Page_Rectangle(page, current_x, y, module_width, height);
    HPDF_Page_Fill(page);
    current_x += module_width;
    
    HPDF_Page_Rectangle(page, current_x, y, module_width, height);
    HPDF_Page_Fill(page);
    current_x += module_width;
    
    // Space between left guard bars
    current_x += module_width;
    
    // Determine left-hand pattern set from first digit
    int first_digit = data[0] - '0';
    const char* pattern_set = ean_first_digit_patterns[first_digit];
    
    // Left-hand digits (positions 1-6)
    for (int i = 1; i <= 6; i++) {
        int digit = data[i] - '0';
        const char* pattern;
        
        if (pattern_set[i-1] == 'A') {
            pattern = ean_left_patterns[digit][0];
        } else {
            pattern = ean_left_patterns[digit][1];
        }
        
        // Group consecutive bars
        int j = 0;
        while (j < 7) {
            if (pattern[j] == '1') {
                // Find consecutive '1's to draw as a single rectangle
                int start = j;
                while (j < 7 && pattern[j] == '1') {
                    j++;
                }
                int consecutive_ones = j - start;
                
                HPDF_Page_Rectangle(page,
                    current_x + (start * module_width),
                    y,
                    consecutive_ones * module_width,
                    height
                );
                HPDF_Page_Fill(page);
            } else {
                j++;
            }
        }
        current_x += 7 * module_width;
    }
    
    // Center guard bars - space before first bar
    current_x += module_width;
    
    // First center guard bar
    HPDF_Page_Rectangle(page, current_x, y, module_width, height);
    HPDF_Page_Fill(page);
    current_x += module_width;
    
    // Space between center guard bars
    current_x += module_width;
    
    // Second center guard bar
    HPDF_Page_Rectangle(page, current_x, y, module_width, height);
    HPDF_Page_Fill(page);
    current_x += module_width;
    
    // Space after center guard bars
    current_x += module_width;
    
    // Right-hand digits (positions 7-12)
    for (int i = 7; i <= 12; i++) {
        int digit = data[i] - '0';
        const char* pattern = ean_right_patterns[digit];
        
        // Group consecutive bars
        int j = 0;
        while (j < 7) {
            if (pattern[j] == '1') {
                // Find consecutive '1's to draw as a single rectangle
                int start = j;
                while (j < 7 && pattern[j] == '1') {
                    j++;
                }
                int consecutive_ones = j - start;
                
                HPDF_Page_Rectangle(page,
                    current_x + (start * module_width),
                    y,
                    consecutive_ones * module_width,
                    height
                );
                HPDF_Page_Fill(page);
            } else {
                j++;
            }
        }
        current_x += 7 * module_width;
    }
    
    // Right guard bars - space before first bar
    current_x += module_width;
    
    // First right guard bar
    HPDF_Page_Rectangle(page, current_x, y, module_width, height);
    HPDF_Page_Fill(page);
    current_x += module_width;
    
    // Space between right guard bars
    current_x += module_width;
    
    // Second right guard bar
    HPDF_Page_Rectangle(page, current_x, y, module_width, height);
    HPDF_Page_Fill(page);
}

void draw_upca(HPDF_Page page, float x, float y, float width, float height, const char* data) {
    if (!validate_barcode_data(BARCODE_UPCA, data)) return;
    
    // Convert UPC-A to EAN-13 format (add leading 0)
    char ean13_data[14];
    snprintf(ean13_data, sizeof(ean13_data), "0%s", data);
    
    // Draw as EAN-13 (UPC-A is a subset of EAN-13)
    draw_ean13(page, x, y, width, height, ean13_data);
}