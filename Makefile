# Textor - Windows 32-bit Text Editor
# Build with MinGW GCC: mingw32-make

CC = gcc
CFLAGS = -O2 -Wall -Wextra -mwindows
LDFLAGS = -mwindows -lgdi32 -lcomdlg32 -luxtheme
TARGET = textor.exe
SRC = textor.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	del /Q $(TARGET) 2>nul || true
