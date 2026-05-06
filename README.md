# Textor – Windows 32-bit Text & Code Editor

A simple, fast plain text editor for Windows, written in **pure C** using the **Windows 32-bit API**. No external frameworks or databases.

## Features

- **File:** New, Open, Save, Save As
- **Edit:** Cut, Copy, Paste, Undo, Select All
- **Line numbers** on the left panel, synced with the text area
- **Light/Dark theme** (View → Light theme / Dark theme)
- **Menu bar:** File, Edit, View, Help
- **Keyboard shortcuts:** Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+X, Ctrl+C, Ctrl+V, Ctrl+Z, Ctrl+A

## Requirements

- **OS:** Windows 7 / 8 / 10 / 11 (32-bit supported)
- **Compiler:** GCC / MinGW
- **Hardware:** ~2 GB RAM, ~50–100 MB free disk

## Build

Using MinGW (e.g. from MSYS2 or standalone MinGW):

```bash
mingw32-make
```

Or with `gcc` directly:

```bash
gcc -O2 -Wall -mwindows -o textor.exe textor.c
```

## Run

```bash
textor.exe
```

Optional: open a file from the command line:

```bash
textor.exe "C:\path\to\file.txt"
```

## Project layout

- `textor.c` – single source file (UI, file handling, edit engine, line numbers, menus)
- `Makefile` – build for MinGW
- `README.md` – this file

## Authors

- Mujahid Al Mahi  
- Shahariar Hossain Jibon  
- Khondokar Ahnaf Elahe  
