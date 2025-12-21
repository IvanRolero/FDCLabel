# FDCLabel

A fast bare bones label generator

Usage: FDCLabel.exe <csv_file> [options]

Required:
  csv_file              Path to CSV data file

Options:

-c, --config FILE     JSON configuration file (default: config.json)

  -o, --output FILE     Output PDF filename (default: labels.pdf)
	
  -r, --row INDEX       Process specific row only (default: all rows)
	
  --validate            Validate configuration without generating PDF
	
  -v, --version         Show version information
	
  -h, --help            Show this message

# Examples:

  FDCLabel.exe data.csv                      (Use defaults)
	
  FDCLabel.exe data.csv -c config1.json      (Custom config)
	
  FDCLabel.exe data.csv -o output.pdf -r 5   (Specific output and row)
	
  FDCLabel.exe data.csv --validate           (Validate config only)
	
  FDCLabel.exe -c shipping.json shipping.csv  (Generate PDF from shipping.json configuration file and shipping.csv information file)
	


# CSV File Format Structure

First row must contain field names (headers)

Supports quoted fields with "" for escaping quotes

Maximum: 256 fields, 100000 rows

Maximum line length: 8192 characters


## Sample CSV
Product,SKU,Price,Description

"Widget A","ABC-123",19.99,"Premium quality widget"

"Widget B","DEF-456",29.99,"Deluxe version with extras"

# Sample JSON

{
    "page": {
        "size": "A4",
        "orientation": "landscape",
        "line_width": 3.0
    },
    "fonts": {
        "default": "Helvetica-Bold",
        "custom_fonts": [
            {
                "name": "MyFont",
                "file": "/path/to/font.ttf",
                "encoding": "WinAnsiEncoding"
            }
        ]
    },
    "fields": [...],
    "lines": [...],
    "qr_code": {...},
    "barcodes": [...]
}


# Page Configuration values

Property	Values	Default
size:	"A3", "A4", "A5", "LETTER", "LEGAL"	"A4"
orientation:	"portrait", "landscape"
line_width:	Positive number	3.0 (dots)

Font Configuration

Default Fonts:

-Courier
-Courier-Bold
-Courier-Oblique
-Courier-BoldOblique
-Helvetica
-Helvetica-Bold
-Helvetica-Oblique
-Helvetica-BoldOblique
-Times-Roman
-Times-Bold
-Times-Italic
-Times-BoldItalic
-Symbol
-ZapfDingbats


Custom Fonts:


"custom_fonts": [
    {
        "name": "CustomFontName",
        "file": "path/to/font.ttf",
        "encoding": "WinAnsiEncoding"
    }
]


## Field Configuration

x_start, x_end (coordinates Positive number in dots)
y_start, y_end (coordinates Positive number in dots)
font_size	(Positive number in dots)
text (alphanumeric text using $ as a )
font_name	F(ont name to use	Default font)
wrap	(Enable text wrapping (0/1)
align	"left", "center", "right"
max_length (Maximum characters used to truncate text) (0=unlimited)


Text Template Variables:

$FieldName: Insert CSV column value

HEX_CODE or RANDOM_HEX: Generate 10-character random hex code
    
    
Example Field:
json

{
    "x_start": 50,
    "x_end": 200,
    "y_start": 700,
    "y_end": 730,
    "font_size": 16,
    "font_name": "Helvetica-Bold",
    "text": "$Product",
    "wrap": 1,
    "align": "center",
    "max_length": 50
}

CSV file Sample

Product,price
anchor,$125

## Line Configuration

Line Types:

-Raw lines: Specify start/end coordinates
-Horizontal transform lines: Single Y coordinate with start/end X

Example Lines:
json

"lines": [
    {
        "type": "horizontal_transform",
        "x_start": 0,
        "x_end": 841.89,
        "y": 400,
        "width": 2.0
    },
    {
        "x_start": 100,
        "y_start": 100,
        "x_end": 300,
        "y_end": 300,
        "width": 1.5
    }
]

## QR Code Configuration

Property	Required	Description	Default
x, y	No	Position in dots	192.0, 1.0
size	No	Size in dots	113.4
text	No	Content (supports templates)	""
enabled	No	Enable/disable QR code	true


json

"qr_code": {
    "x": 192.0,
    "y": 1.0,
    "size": 113.4,
    "text": "$SKU",
    "enabled": true
}


## Barcode Configuration

Supported types: "code128", "ean13", "upca"

Example:
json

"barcodes": [
    {
        "x": 50,
        "y": 50,
        "width": 100,
        "height": 50,
        "type": "code128",
        "text": "$SKU"
    }
]


Coordinate System (libharou system)

Origin (0,0) is at bottom-left of page
Units are in points (1 point = 1/72 inch)
A4 landscape: 841.89 × 595.28 points
A4 portrait: 595.28 × 841.89 points    
    
Text Wrapping Behavior

When wrap is enabled:

Text automatically fits within the defined box
Font size may be reduced to fit
Text is broken into multiple lines
Alignment options are preserved
    
    
Error Handling
Common Errors:

Missing CSV file: Error: CSV file is required
Invalid JSON: Shows position of parse error
Missing required fields: Warns about missing coordinates
Font file not found: Warning during validation
    
    
## Troubleshooting

Fonts not displaying:	Check font file paths, use absolute paths
Text overflowing:	Enable wrap or reduce font_size
CSV fields not substituted:	Verify column names match exactly (case-sensitive)
PDF not generated:	Check file permissions, disk space
Lines not appearing:	Verify coordinates are within page bounds
QR code too small:	Increase size parameter
Wrong characters displayed, no accents: CSV and JSON config files must be ANSI coded
