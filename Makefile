# ==== Compiler and Flags ====
CC = gcc
CFLAGS = -O2 -Wall -Wextra \
    -Ilibs/cJSON \
    -Ilibs/Qrcodegen \
    -Ilibs/Barcodes \
    -Ilibs/Libharu/include \
    -Ilibs/Libharu/build/include

LDFLAGS = -lm -lz -static

# ==== Paths ====
LIBHPDF = libs/Libharu/build/src/libhpdf.a
SRC = src/FDCLabel_main.c src/FDCLabel_utils.c libs/cJSON/cJSON.c libs/Qrcodegen/qrcodegen.c libs/Barcodes/barcodes.c
OBJ = $(SRC:.c=.o)
TARGET = FDCLabel.exe

# ==== Default rule ====
all: $(TARGET)

# ==== Main build ====
$(TARGET): $(LIBHPDF) $(OBJ)
	@echo Linking $@ ...
	$(CC) -o $@ $(OBJ) $(LIBHPDF) $(LDFLAGS)
	@echo Build complete: $@

# ==== Compile source ====
%.o: %.c
	@echo Compiling $< ...
	$(CC) $(CFLAGS) -c $< -o $@

# ==== Automatically build libharu if missing ====
$(LIBHPDF):
	@echo Building libharu ...
	@if not exist "libs\Libharu\build" mkdir libs\Libharu\build
	cd libs\Libharu && cmake -B build -G "MinGW Makefiles" -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DLIBHPDF_EXAMPLES=OFF -DLIBHPDF_TEST=OFF
	cd libs\Libharu && cmake --build build

# ==== Clean ====
clean:
	@echo Cleaning ...
	-del /Q $(OBJ) $(TARGET) 2>nul || true

distclean: clean
	@echo Removing libharu build ...
	-rd /S /Q libs\Libharu\build 2>nul || true