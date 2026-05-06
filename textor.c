/* ============================================================
   Textor - A lightweight Windows text/code editor
   Built with the Win32 API in pure C.
   ============================================================ */

/* --- Windows targeting macros --- */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501
#ifndef EM_GETSELTEXT
#define EM_GETSELTEXT 0x043E
#endif

/* --- Standard and Windows headers --- */
#include <windows.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
   Control and Menu IDs
   IDC_*  : child window control identifiers
   IDM_*  : menu command identifiers
   ID_*   : dialog control identifiers
   ============================================================ */
#define IDC_EDIT 101
#define IDC_LINENUM 102
#define IDC_STATUSBAR 103
#define IDM_FILE_NEW 201
#define IDM_FILE_OPEN 202
#define IDM_FILE_SAVE 203
#define IDM_FILE_SAVEAS 204
#define IDM_FILE_EXIT 205
#define IDM_EDIT_CUT 211
#define IDM_EDIT_COPY 212
#define IDM_EDIT_PASTE 213
#define IDM_EDIT_UNDO 214
#define IDM_EDIT_REDO 215
#define IDM_EDIT_SELALL 216
#define IDM_EDIT_FIND 217
#define IDM_EDIT_REPLACE 219
#define IDM_EDIT_GOTOLINE 218
#define IDM_EDIT_INDENT 225
#define IDM_VIEW_LIGHT 220
#define IDM_VIEW_DARK 221
#define IDM_VIEW_WORDWRAP 226
#define IDM_VIEW_FONT_SMALL 222
#define IDM_VIEW_FONT_MEDIUM 223
#define IDM_VIEW_FONT_LARGE 224
#define IDM_VIEW_ZOOM_IN 228
#define IDM_VIEW_ZOOM_OUT 229
#define IDM_HELP_ABOUT 230
#define ID_FIND_EDIT 301
#define ID_FIND_NEXT 302
#define ID_FIND_CANCEL 303
#define ID_GOTO_EDIT 401
#define ID_GOTO_GO 402
/* --- Font size presets (in logical pixels) --- */
#define FONT_SIZE_SMALL 12
#define FONT_SIZE_MEDIUM 16
#define FONT_SIZE_LARGE 20
#define FONT_SIZE_TINY 10
#define FONT_SIZE_HUGE 32
/* --- Editor layout / undo limits --- */
#define MAX_UNDO 200
#define LINENUM_WIDTH 56
#define MARGIN 4
#define STATUSBAR_HEIGHT 22

/* ============================================================
   Global state
   All editor-wide state is stored in g_* globals so every
   function can access it without parameter threading.
   ============================================================ */

/* --- Window handles --- */
HWND g_hWndMain, g_hEdit, g_hLineNum, g_hStatusBar;
/* --- Currently open file --- */
char g_szFilePath[MAX_PATH] = "";
char g_szFileName[MAX_PATH] = "Untitled";
/* --- Editor flags --- */
BOOL g_bModified = FALSE;
BOOL g_bDarkTheme = TRUE;
BOOL g_bWordWrap = FALSE;
BOOL g_bRestoringUndo = FALSE;
int g_nFontSize = FONT_SIZE_MEDIUM;
char g_szReplaceBuf[256] = "";
/* --- GDI resources --- */
HFONT g_hFont = NULL;
WNDPROC g_lpfnEditProc = NULL;
HBRUSH g_hBrushEditBg = NULL;
HBRUSH g_hBrushLineNumBg = NULL;
/* --- Theme colours (updated by ApplyTheme) --- */
COLORREF g_clrEditBg = RGB(30, 30, 32);
COLORREF g_clrEditText = RGB(212, 212, 212);
COLORREF g_clrLineNumBg = RGB(45, 45, 48);
COLORREF g_clrLineNumText = RGB(160, 160, 160);
/* --- Custom undo/redo stacks (array of heap-allocated text snapshots) --- */
static char *g_undoStack[MAX_UNDO];
static int g_undoCount = 0;
static char *g_redoStack[MAX_UNDO];
static int g_redoCount = 0;
/* --- Find/Replace state --- */
static char g_szFindBuf[256] = "";
static HWND g_hFindWnd = NULL;

/* ============================================================
   Forward declarations
   ============================================================ */
HMENU CreateMainMenu(void);
void UpdateWindowTitle(HWND hwnd);
BOOL DoOpenFile(HWND hwnd, const char *path);
BOOL DoSaveFile(HWND hwnd, const char *path);
BOOL PromptSaveIfModified(HWND hwnd);
void ClearUndoRedo(void);
void PushUndo(void);
void DoUndo(HWND hwnd);
void DoRedo(HWND hwnd);
void PushCurrentToRedo(void);
BOOL SearchInEdit(const char *findStr, BOOL fromStart);
LRESULT CALLBACK FindDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void DoFind(HWND hwndParent);
LRESULT CALLBACK GoToLineDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void DoGoToLine(HWND hwndParent);
LRESULT CALLBACK ReplaceDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void DoReplace(HWND hwndParent);
void ToggleWordWrap(HWND hwnd);
void ApplyTheme(HWND hwnd);
void ApplyFont(HWND hwnd);
void PaintLineNumbers(HDC hdc, RECT *prc);
LRESULT CALLBACK LineNumProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL RegisterLineNumClass(void);
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void UpdateLineNumbers(void);
void UpdateStatusBar(HWND hwnd);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* ============================================================
   CreateMainMenu
   Builds and returns the application menu bar with File, Edit,
   View, and Help sub-menus, including keyboard shortcut hints.
   ============================================================ */
HMENU CreateMainMenu(void)
{
    HMENU hMenu = CreateMenu();
    HMENU hFile = CreateMenu();
    HMENU hEdit = CreateMenu();
    HMENU hHelp = CreateMenu();

    AppendMenu(hFile, MF_STRING, IDM_FILE_NEW, "&New\tCtrl+N");
    AppendMenu(hFile, MF_STRING, IDM_FILE_OPEN, "&Open...\tCtrl+O");
    AppendMenu(hFile, MF_STRING, IDM_FILE_SAVE, "&Save\tCtrl+S");
    AppendMenu(hFile, MF_STRING, IDM_FILE_SAVEAS, "Save &As...");
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING, IDM_FILE_EXIT, "E&xit\tAlt+F4");

    AppendMenu(hEdit, MF_STRING, IDM_EDIT_UNDO, "&Undo\tCtrl+Z");
    AppendMenu(hEdit, MF_STRING, IDM_EDIT_REDO, "&Redo\tCtrl+Y");
    AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEdit, MF_STRING, IDM_EDIT_CUT, "Cu&t\tCtrl+X");
    AppendMenu(hEdit, MF_STRING, IDM_EDIT_COPY, "&Copy\tCtrl+C");
    AppendMenu(hEdit, MF_STRING, IDM_EDIT_PASTE, "&Paste\tCtrl+V");
    AppendMenu(hEdit, MF_STRING, IDM_EDIT_INDENT, "Indent\tTab");
    AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEdit, MF_STRING, IDM_EDIT_SELALL, "Select &All\tCtrl+A");
    AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEdit, MF_STRING, IDM_EDIT_FIND, "&Find...\tCtrl+F");
    AppendMenu(hEdit, MF_STRING, IDM_EDIT_REPLACE, "&Replace...\tCtrl+H");
    AppendMenu(hEdit, MF_STRING, IDM_EDIT_GOTOLINE, "&Go to Line...\tCtrl+G");

    HMENU hView = CreateMenu();
    AppendMenu(hView, MF_STRING, IDM_VIEW_LIGHT, "&Light theme");
    AppendMenu(hView, MF_STRING, IDM_VIEW_DARK, "&Dark theme");
    AppendMenu(hView, MF_SEPARATOR, 0, NULL);
    AppendMenu(hView, MF_STRING, IDM_VIEW_WORDWRAP, "&Word Wrap");
    AppendMenu(hView, MF_SEPARATOR, 0, NULL);
    AppendMenu(hView, MF_STRING, IDM_VIEW_FONT_SMALL, "Small &font");
    AppendMenu(hView, MF_STRING, IDM_VIEW_FONT_MEDIUM, "&Medium font");
    AppendMenu(hView, MF_STRING, IDM_VIEW_FONT_LARGE, "&Large font");

    AppendMenu(hHelp, MF_STRING, IDM_HELP_ABOUT, "&About Textor");

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFile, "&File");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEdit, "&Edit");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hView, "&View");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelp, "&Help");

    return hMenu;
}

/* ============================================================
   UpdateWindowTitle
   Refreshes the title bar to show the current file name and a
   '*' marker when there are unsaved changes.
   ============================================================ */
void UpdateWindowTitle(HWND hwnd)
{
    char title[MAX_PATH + 32];
    if (g_bModified)
        sprintf(title, "%s * - Textor", g_szFileName);
    else
        sprintf(title, "%s - Textor", g_szFileName);
    SetWindowText(hwnd, title);
}

/* ============================================================
   DoOpenFile
   Opens the file at 'path', reads its entire contents into the
   edit control, resets undo/redo history, and updates the title.
   Returns FALSE if the file cannot be opened or read.
   ============================================================ */
BOOL DoOpenFile(HWND hwnd, const char *path)
{
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    DWORD size = GetFileSize(hFile, NULL);
    if (size == 0xFFFFFFFF)
    {
        CloseHandle(hFile);
        return FALSE;
    }

    char *buf = (char *)GlobalAlloc(GPTR, size + 2);
    if (!buf)
    {
        CloseHandle(hFile);
        return FALSE;
    }

    DWORD read;
    if (!ReadFile(hFile, buf, size, &read, NULL))
    {
        CloseHandle(hFile);
        GlobalFree(buf);
        return FALSE;
    }
    CloseHandle(hFile);

    buf[read] = '\0';

    SetWindowText(g_hEdit, buf);
    SendMessage(g_hEdit, EM_EMPTYUNDOBUFFER, 0, 0);
    ClearUndoRedo();
    GlobalFree(buf);

    strcpy(g_szFilePath, path);
    strcpy(g_szFileName, strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path);
    g_bModified = FALSE;
    UpdateWindowTitle(hwnd);
    UpdateLineNumbers();
    return TRUE;
}

/* ============================================================
   DoSaveFile
   Writes the current edit-control text to 'path', then clears
   the modified flag and refreshes the window title.
   Returns FALSE if the file cannot be created or written.
   ============================================================ */
BOOL DoSaveFile(HWND hwnd, const char *path)
{
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    int len = GetWindowTextLength(g_hEdit);
    if (len > 0)
    {
        char *buf = (char *)GlobalAlloc(GPTR, len + 1);
        if (buf)
        {
            GetWindowText(g_hEdit, buf, len + 1);
            DWORD written;
            WriteFile(hFile, buf, (DWORD)len, &written, NULL);
            GlobalFree(buf);
        }
    }
    CloseHandle(hFile);

    strcpy(g_szFilePath, path);
    strcpy(g_szFileName, strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path);
    g_bModified = FALSE;
    UpdateWindowTitle(hwnd);
    return TRUE;
}

/* ============================================================
   PromptSaveIfModified
   If the buffer has unsaved changes, asks the user Yes/No/Cancel.
   - Yes  : saves (showing Save As dialog if no path is set).
   - No   : discards changes.
   - Cancel: aborts the calling operation.
   Returns FALSE only when the user cancels.
   ============================================================ */
BOOL PromptSaveIfModified(HWND hwnd)
{
    if (!g_bModified)
        return TRUE;
    int r = MessageBoxA(hwnd, "Do you want to save changes?", "Textor",
                        MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL)
        return FALSE;
    if (r == IDNO)
        return TRUE;
    if (g_szFilePath[0])
        return DoSaveFile(hwnd, g_szFilePath);
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = g_szFilePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "txt";
    if (!GetSaveFileNameA(&ofn))
        return FALSE;
    return DoSaveFile(hwnd, g_szFilePath);
}

/* ============================================================
   ClearUndoRedo
   Frees all heap-allocated text snapshots in both the undo and
   redo stacks and resets their counters to zero.
   ============================================================ */
void ClearUndoRedo(void)
{
    int i;
    for (i = 0; i < g_undoCount; i++)
        free(g_undoStack[i]);
    g_undoCount = 0;
    for (i = 0; i < g_redoCount; i++)
        free(g_redoStack[i]);
    g_redoCount = 0;
}

/* ============================================================
   PushUndo
   Takes a snapshot of the current edit-control text and pushes
   it onto the undo stack.  Duplicate consecutive snapshots are
   discarded.  When the stack is full the oldest entry is dropped.
   ============================================================ */
void PushUndo(void)
{
    if (!g_hEdit || !IsWindow(g_hEdit) || g_bRestoringUndo)
        return;
    int len = GetWindowTextLength(g_hEdit);
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf)
        return;
    GetWindowText(g_hEdit, buf, len + 1);
    if (g_undoCount > 0)
    {
        if (strcmp(g_undoStack[g_undoCount - 1], buf) == 0)
        {
            free(buf);
            return;
        }
    }
    if (g_undoCount >= MAX_UNDO)
    {
        free(g_undoStack[0]);
        memmove(&g_undoStack[0], &g_undoStack[1], (size_t)(g_undoCount - 1) * sizeof(char *));
        g_undoCount--;
    }
    g_undoStack[g_undoCount++] = buf;
}

/* Frees all redo snapshots — called whenever new text is typed
   so that a redo after an edit is not possible. */
static void ClearRedo(void)
{
    int i;
    for (i = 0; i < g_redoCount; i++)
        free(g_redoStack[i]);
    g_redoCount = 0;
}

/* ============================================================
   DoUndo
   Pops the most recent undo snapshot, saves the current text to
   the redo stack, restores the snapshot into the edit control,
   and refreshes the UI.
   ============================================================ */
void DoUndo(HWND hwnd)
{
    if (g_undoCount == 0)
        return;
    PushCurrentToRedo();
    char *prev = g_undoStack[--g_undoCount];
    g_bRestoringUndo = TRUE;
    SetWindowText(g_hEdit, prev);
    SendMessage(g_hEdit, EM_EMPTYUNDOBUFFER, 0, 0);
    SendMessage(g_hEdit, EM_SETSEL, 0, 0);
    g_bRestoringUndo = FALSE;
    free(prev);
    g_bModified = TRUE;
    UpdateWindowTitle(hwnd);
    UpdateLineNumbers();
    UpdateStatusBar(hwnd);
}

/* ============================================================
   DoRedo
   Pops the top redo snapshot, pushes the current text onto the
   undo stack, and restores the snapshot into the edit control.
   ============================================================ */
void DoRedo(HWND hwnd)
{
    if (g_redoCount == 0)
        return;
    PushUndo();
    char *next = g_redoStack[--g_redoCount];
    g_bRestoringUndo = TRUE;
    SetWindowText(g_hEdit, next);
    SendMessage(g_hEdit, EM_EMPTYUNDOBUFFER, 0, 0);
    SendMessage(g_hEdit, EM_SETSEL, 0, 0);
    g_bRestoringUndo = FALSE;
    free(next);
    g_bModified = TRUE;
    UpdateWindowTitle(hwnd);
    UpdateLineNumbers();
    UpdateStatusBar(hwnd);
}

/* ============================================================
   PushCurrentToRedo
   Saves the current edit-control contents to the redo stack.
   Called by DoUndo before it overwrites the edit control.
   ============================================================ */
void PushCurrentToRedo(void)
{
    if (!g_hEdit || !IsWindow(g_hEdit))
        return;
    int len = GetWindowTextLength(g_hEdit);
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf)
        return;
    GetWindowText(g_hEdit, buf, len + 1);
    if (g_redoCount >= MAX_UNDO)
    {
        free(g_redoStack[0]);
        memmove(&g_redoStack[0], &g_redoStack[1], (size_t)(g_redoCount - 1) * sizeof(char *));
        g_redoCount--;
    }
    g_redoStack[g_redoCount++] = buf;
}

/* ============================================================
   SearchInEdit
   Searches the edit-control text for 'findStr'.
   If 'fromStart' is FALSE the search begins after the current
   selection end; if TRUE it restarts from the beginning.
   On a match the text is selected and scrolled into view.
   Returns FALSE when the string is not found.
   ============================================================ */
BOOL SearchInEdit(const char *findStr, BOOL fromStart)
{
    if (!findStr || !*findStr || !g_hEdit)
        return FALSE;
    int len = GetWindowTextLength(g_hEdit);
    if (len <= 0)
        return FALSE;
    char *text = (char *)malloc((size_t)len + 1);
    if (!text)
        return FALSE;
    GetWindowText(g_hEdit, text, len + 1);
    int startSel = 0, endSel = 0;
    SendMessage(g_hEdit, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);
    int start = fromStart ? 0 : endSel;
    if (start < 0)
        start = 0;
    if (start >= len)
        start = 0;
    const char *p = strstr(text + start, findStr);
    if (!p && start > 0)
        p = strstr(text, findStr);
    if (!p)
    {
        free(text);
        return FALSE;
    }
    int pos = (int)(p - text);
    int endPos = pos + (int)strlen(findStr);
    free(text);
    SendMessage(g_hEdit, EM_SETSEL, (WPARAM)pos, (LPARAM)endPos);
    SendMessage(g_hEdit, EM_SCROLLCARET, 0, 0);
    return TRUE;
}

/* ============================================================
   FindDlgProc
   Window procedure for the modeless Find dialog.
   WM_CREATE  : builds the search box and buttons.
   WM_COMMAND : "Find Next" triggers SearchInEdit; "Cancel" closes.
   ============================================================ */
LRESULT CALLBACK FindDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg)
    {
    case WM_CREATE:
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_szFindBuf,
                        WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 10, 260, 22,
                        hwnd, (HMENU)ID_FIND_EDIT, GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "Find &Next",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 38, 90, 24,
                        hwnd, (HMENU)ID_FIND_NEXT, GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "Cancel",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 105, 38, 70, 24,
                        hwnd, (HMENU)ID_FIND_CANCEL, GetModuleHandle(NULL), NULL);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_FIND_NEXT)
        {
            char buf[256];
            GetWindowTextA(GetDlgItem(hwnd, ID_FIND_EDIT), buf, 255);
            buf[255] = '\0';
            strcpy(g_szFindBuf, buf);
            if (!SearchInEdit(g_szFindBuf, FALSE))
            {
                if (!SearchInEdit(g_szFindBuf, TRUE))
                    MessageBoxA(hwnd, "Cannot find the specified text.", "Find", MB_OK | MB_ICONINFORMATION);
            }
            SetFocus(g_hEdit);
            return 0;
        }
        if (LOWORD(wParam) == ID_FIND_CANCEL)
        {
            g_hFindWnd = NULL;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        g_hFindWnd = NULL;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_hFindWnd = NULL;
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* ============================================================
   DoFind
   Opens (or brings to the foreground) the modeless Find dialog.
   Registers the window class on the first call.
   ============================================================ */
void DoFind(HWND hwndParent)
{
    if (g_hFindWnd && IsWindow(g_hFindWnd))
    {
        SetForegroundWindow(g_hFindWnd);
        return;
    }
    WNDCLASSEXA wc = {sizeof(wc)};
    wc.lpfnWndProc = FindDlgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "TextorFindDlg";
    RegisterClassExA(&wc);
    g_hFindWnd = CreateWindowExA(WS_EX_DLGMODALFRAME, "TextorFindDlg", "Find",
                                 WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                 100, 100, 285, 95,
                                 hwndParent, NULL, GetModuleHandle(NULL), NULL);
    if (g_hFindWnd)
    {
        ShowWindow(g_hFindWnd, SW_SHOW);
        SetFocus(GetDlgItem(g_hFindWnd, ID_FIND_EDIT));
    }
}

/* ============================================================
   GoToLineDlgProc
   Window procedure for the Go-to-Line dialog.
   WM_CREATE  : creates a number input box and Go/Cancel buttons.
   WM_COMMAND : "Go" moves the caret to the requested line number.
   ============================================================ */
LRESULT CALLBACK GoToLineDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg)
    {
    case WM_CREATE:
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "1",
                        WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 10, 200, 22,
                        hwnd, (HMENU)ID_GOTO_EDIT, GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "&Go",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 38, 60, 24,
                        hwnd, (HMENU)ID_GOTO_GO, GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "Cancel",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 75, 38, 70, 24,
                        hwnd, (HMENU)ID_FIND_CANCEL, GetModuleHandle(NULL), NULL);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_GOTO_GO)
        {
            char buf[32];
            GetWindowTextA(GetDlgItem(hwnd, ID_GOTO_EDIT), buf, 31);
            buf[31] = '\0';
            int line = atoi(buf);
            int lineCount = (int)SendMessage(g_hEdit, EM_GETLINECOUNT, 0, 0);
            if (lineCount <= 0)
                lineCount = 1;
            if (line >= 1 && line <= lineCount)
            {
                int pos = (int)SendMessage(g_hEdit, EM_LINEINDEX, line - 1, 0);
                SendMessage(g_hEdit, EM_SETSEL, (WPARAM)pos, (LPARAM)pos);
                SendMessage(g_hEdit, EM_SCROLLCARET, 0, 0);
                UpdateStatusBar(GetParent(g_hEdit));
            }
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wParam) == ID_FIND_CANCEL)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* ============================================================
   DoGoToLine
   Creates and shows the Go-to-Line dialog.
   ============================================================ */
void DoGoToLine(HWND hwndParent)
{
    WNDCLASSEXA wc = {sizeof(wc)};
    wc.lpfnWndProc = GoToLineDlgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "TextorGoToLineDlg";
    RegisterClassExA(&wc);
    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "TextorGoToLineDlg", "Go to Line",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                150, 150, 225, 95,
                                hwndParent, NULL, GetModuleHandle(NULL), NULL);
    if (hDlg)
        ShowWindow(hDlg, SW_SHOW);
}

/* ============================================================
   ReplaceDlgProc
   Window procedure for the Find & Replace dialog.
   WM_CREATE  : builds two text boxes (find / replace) and three
                action buttons.
   WM_COMMAND :
     303 - Find Next      : advance to next match.
     304 - Replace        : replace the current selection if it
                            matches, then find next.
     305 - Replace All    : iterates the whole document replacing
                            every occurrence and reports the count.
   ============================================================ */
LRESULT CALLBACK ReplaceDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg)
    {
    case WM_CREATE:
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_szFindBuf,
                        WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 10, 260, 22,
                        hwnd, (HMENU)301, GetModuleHandle(NULL), NULL);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_szReplaceBuf,
                        WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 38, 260, 22,
                        hwnd, (HMENU)302, GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "Find &Next",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 66, 80, 24,
                        hwnd, (HMENU)303, GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "&Replace",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 95, 66, 80, 24,
                        hwnd, (HMENU)304, GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "Replace &All",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 180, 66, 90, 24,
                        hwnd, (HMENU)305, GetModuleHandle(NULL), NULL);
        CreateWindowExA(0, "BUTTON", "Cancel",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 275, 66, 70, 24,
                        hwnd, (HMENU)306, GetModuleHandle(NULL), NULL);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == 303 || LOWORD(wParam) == 304 || LOWORD(wParam) == 305)
        {
            char findBuf[256], replaceBuf[256];
            GetWindowTextA(GetDlgItem(hwnd, 301), findBuf, 255);
            findBuf[255] = '\0';
            GetWindowTextA(GetDlgItem(hwnd, 302), replaceBuf, 255);
            replaceBuf[255] = '\0';
            strcpy(g_szFindBuf, findBuf);
            strcpy(g_szReplaceBuf, replaceBuf);

            if (LOWORD(wParam) == 303)
            {
                if (!SearchInEdit(findBuf, FALSE))
                    SearchInEdit(findBuf, TRUE);
            }
            else if (LOWORD(wParam) == 304)
            {
                int len = GetWindowTextLength(g_hEdit);
                char *text = (char *)malloc(len + 1);
                if (text)
                {
                    GetWindowText(g_hEdit, text, len + 1);
                    int start = 0, end = 0;
                    SendMessage(g_hEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
                    int selLen = end - start;
                    if (selLen > 0 && selLen <= len && start >= 0 && end <= len)
                    {
                        if (strncmp(text + start, findBuf, selLen) == 0)
                        {
                            PushUndo();
                            SendMessage(g_hEdit, EM_REPLACESEL, TRUE, (LPARAM)replaceBuf);
                            g_bModified = TRUE;
                            ClearRedo();
                        }
                    }
                    free(text);
                }
                if (!SearchInEdit(findBuf, FALSE))
                    SearchInEdit(findBuf, TRUE);
            }
            else if (LOWORD(wParam) == 305)
            {
                int count = 0;
                int totalLen = GetWindowTextLength(g_hEdit);
                char *text = (char *)malloc(totalLen + 1);
                if (!text)
                {
                    MessageBoxA(hwnd, "Memory error.", "Replace All", MB_OK);
                    SetFocus(g_hEdit);
                    return 0;
                }
                GetWindowText(g_hEdit, text, totalLen + 1);

                PushUndo();
                char *pos = text;
                while ((pos = strstr(pos, findBuf)) != NULL)
                {
                    int matchPos = (int)(pos - text);
                    SendMessage(g_hEdit, EM_SETSEL, matchPos, matchPos + strlen(findBuf));
                    SendMessage(g_hEdit, EM_REPLACESEL, TRUE, (LPARAM)replaceBuf);
                    count++;

                    totalLen = GetWindowTextLength(g_hEdit);
                    free(text);
                    text = (char *)malloc(totalLen + 1);
                    if (!text)
                        break;
                    GetWindowText(g_hEdit, text, totalLen + 1);
                    pos = text + matchPos + strlen(replaceBuf);
                }
                free(text);
                if (count > 0)
                {
                    g_bModified = TRUE;
                    ClearRedo();
                }

                char msgBuf[64];
                sprintf(msgBuf, "Replaced %d occurrence(s).", count);
                MessageBoxA(hwnd, msgBuf, "Replace All", MB_OK);
            }
            SetFocus(g_hEdit);
            return 0;
        }
        if (LOWORD(wParam) == 306)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* ============================================================
   DoReplace
   Creates and shows the Find & Replace dialog.
   ============================================================ */
void DoReplace(HWND hwndParent)
{
    WNDCLASSEXA wc = {sizeof(wc)};
    wc.lpfnWndProc = ReplaceDlgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "TextorReplaceDlg";
    RegisterClassExA(&wc);
    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "TextorReplaceDlg", "Replace",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                100, 100, 370, 145,
                                hwndParent, NULL, GetModuleHandle(NULL), NULL);
    if (hDlg)
        ShowWindow(hDlg, SW_SHOW);
}

/* ============================================================
   ToggleWordWrap
   Flips word-wrap mode on/off by adding or removing the
   ES_AUTOHSCROLL style on the edit control, and updates the
   View menu check mark accordingly.
   ============================================================ */
void ToggleWordWrap(HWND hwnd)
{
    /*
     * ES_AUTOHSCROLL is a creation-time style — SetWindowLong cannot change
     * it on a live EDIT control. The only reliable fix is to destroy the old
     * control and create a new one with the correct style flags.
     */
    g_bWordWrap = !g_bWordWrap;

    /* 1. Save current text */
    int len = GetWindowTextLength(g_hEdit);
    char *savedText = (char *)malloc((size_t)len + 1);
    if (savedText)
        GetWindowText(g_hEdit, savedText, len + 1);
    else
        len = 0;

    /* 2. Save caret position */
    DWORD selStart = 0, selEnd = 0;
    SendMessage(g_hEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

    /* 3. Get current position/size */
    RECT rc;
    GetWindowRect(g_hEdit, &rc);
    ScreenToClient(hwnd, (POINT *)&rc);
    ScreenToClient(hwnd, (POINT *)&rc.right);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    /* 4. Remove subclass before destroying */
    if (g_lpfnEditProc)
        SetWindowLongPtr(g_hEdit, GWLP_WNDPROC, (LONG_PTR)g_lpfnEditProc);

    /* 5. Destroy old edit control */
    DestroyWindow(g_hEdit);
    g_hEdit = NULL;

    /* 6. Build new style */
    DWORD baseStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                      ES_MULTILINE | ES_AUTOVSCROLL |
                      ES_NOHIDESEL | ES_WANTRETURN;
    if (!g_bWordWrap)
        baseStyle |= WS_HSCROLL | ES_AUTOHSCROLL;

    /* 7. Create new edit control */
    g_hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", NULL,
                              baseStyle,
                              x, y, w, h,
                              hwnd, (HMENU)IDC_EDIT, GetModuleHandle(NULL), NULL);

    /* 8. Restore font */
    if (g_hFont)
        SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFont, FALSE);

    /* 9. Re-subclass */
    g_lpfnEditProc = (WNDPROC)SetWindowLongPtr(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

    /* 10. Restore text and caret */
    if (savedText)
    {
        SetWindowText(g_hEdit, savedText);
        free(savedText);
    }
    SendMessage(g_hEdit, EM_SETSEL, selStart, selEnd);
    SendMessage(g_hEdit, EM_SCROLLCARET, 0, 0);

    /* 11. Apply theme colours to new control */
    if (g_bDarkTheme)
        SetWindowTheme(g_hEdit, L"DarkMode_Explorer", NULL);
    else
        SetWindowTheme(g_hEdit, L"", NULL);

    SetFocus(g_hEdit);
    UpdateLineNumbers();
    UpdateStatusBar(hwnd);

    /* 12. Update menu check mark */
    HMENU hMenu = GetMenu(hwnd);
    if (hMenu)
    {
        HMENU hView = GetSubMenu(hMenu, 2);
        if (hView)
            CheckMenuItem(hView, IDM_VIEW_WORDWRAP, MF_BYCOMMAND | (g_bWordWrap ? MF_CHECKED : MF_UNCHECKED));
    }
}

/* ============================================================
   ApplyTheme
   Applies the current dark/light colour scheme:
   - Recreates the background brushes for the edit and gutter.
   - Updates the colour reference globals used in WM_CTLCOLORxxx.
   - Redraws all affected controls and updates menu check marks.
   - Applies the Windows dark-mode visual style to the scrollbar.
   ============================================================ */
void ApplyTheme(HWND hwnd)
{
    if (g_hBrushEditBg)
    {
        DeleteObject(g_hBrushEditBg);
        g_hBrushEditBg = NULL;
    }
    if (g_hBrushLineNumBg)
    {
        DeleteObject(g_hBrushLineNumBg);
        g_hBrushLineNumBg = NULL;
    }
    if (g_bDarkTheme)
    {
        g_clrEditBg = RGB(30, 30, 32);
        g_clrEditText = RGB(212, 212, 212);
        g_clrLineNumBg = RGB(45, 45, 48);
        g_clrLineNumText = RGB(160, 160, 160);
    }
    else
    {
        g_clrEditBg = RGB(255, 255, 255);
        g_clrEditText = RGB(0, 0, 0);
        g_clrLineNumBg = GetSysColor(COLOR_3DFACE);
        g_clrLineNumText = GetSysColor(COLOR_WINDOWTEXT);
    }
    g_hBrushEditBg = CreateSolidBrush(g_clrEditBg);
    g_hBrushLineNumBg = CreateSolidBrush(g_clrLineNumBg);
    if (g_hEdit && IsWindow(g_hEdit))
    {
        InvalidateRect(g_hEdit, NULL, TRUE);
        UpdateWindow(g_hEdit);
    }
    if (g_hLineNum && IsWindow(g_hLineNum))
    {
        InvalidateRect(g_hLineNum, NULL, TRUE);
        UpdateWindow(g_hLineNum);
    }
    if (g_hStatusBar && IsWindow(g_hStatusBar))
    {
        InvalidateRect(g_hStatusBar, NULL, TRUE);
        UpdateWindow(g_hStatusBar);
    }
    HMENU hMenu = GetMenu(hwnd);
    if (hMenu)
    {
        HMENU hView = GetSubMenu(hMenu, 2);
        if (hView)
        {
            CheckMenuItem(hView, IDM_VIEW_LIGHT, MF_BYCOMMAND | (g_bDarkTheme ? MF_UNCHECKED : MF_CHECKED));
            CheckMenuItem(hView, IDM_VIEW_DARK, MF_BYCOMMAND | (g_bDarkTheme ? MF_CHECKED : MF_UNCHECKED));
        }
        MENUINFO mi = {sizeof(mi), MIM_BACKGROUND, MFT_STRING, 0, NULL, 0, 0};
        mi.hbrBack = CreateSolidBrush(g_bDarkTheme ? RGB(51, 51, 55) : GetSysColor(COLOR_MENU));
        SetMenuInfo(hMenu, &mi);
        DrawMenuBar(hwnd);
    }
    if (g_hEdit && IsWindow(g_hEdit))
    {
        if (g_bDarkTheme)
            SetWindowTheme(g_hEdit, L"DarkMode_Explorer", NULL);
        else
            SetWindowTheme(g_hEdit, L"", NULL);
    }
}

/* ============================================================
   ApplyFont
   Creates a new Consolas HFONT at the current g_nFontSize,
   applies it to the edit control and the line-number gutter,
   and updates the font-size check marks in the View menu.
   ============================================================ */
void ApplyFont(HWND hwnd)
{
    (void)hwnd;
    if (g_hFont)
    {
        DeleteObject(g_hFont);
        g_hFont = NULL;
    }
    g_hFont = CreateFontA(g_nFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    if (g_hFont)
    {
        SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        SendMessage(g_hLineNum, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        UpdateLineNumbers();
    }
    HMENU hMenu = GetMenu(g_hWndMain);
    if (hMenu)
    {
        HMENU hView = GetSubMenu(hMenu, 2);
        if (hView)
        {
            CheckMenuItem(hView, IDM_VIEW_FONT_SMALL, MF_BYCOMMAND | (g_nFontSize == FONT_SIZE_SMALL ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(hView, IDM_VIEW_FONT_MEDIUM, MF_BYCOMMAND | (g_nFontSize == FONT_SIZE_MEDIUM ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(hView, IDM_VIEW_FONT_LARGE, MF_BYCOMMAND | (g_nFontSize == FONT_SIZE_LARGE ? MF_CHECKED : MF_UNCHECKED));
        }
    }
}

/* ============================================================
   PaintLineNumbers
   Paints the line-number gutter onto 'hdc'.
   Fills the gutter background, then iterates from the first
   visible line downward, drawing each 1-based line number
   right-aligned until the bottom of the control is reached.
   ============================================================ */
void PaintLineNumbers(HDC hdc, RECT *prc)
{
    SetBkColor(hdc, g_clrLineNumBg);
    SetTextColor(hdc, g_clrLineNumText);
    ExtTextOutA(hdc, 0, 0, ETO_OPAQUE, prc, "", 0, NULL);

    if (!g_hEdit || !IsWindow(g_hEdit))
        return;

    int lineCount = (int)SendMessage(g_hEdit, EM_GETLINECOUNT, 0, 0);
    if (lineCount <= 0)
        lineCount = 1;

    int firstVisible = (int)SendMessage(g_hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
    TEXTMETRIC tm;
    HFONT hFont = (HFONT)SendMessage(g_hEdit, WM_GETFONT, 0, 0);
    HGDIOBJ old = SelectObject(hdc, hFont ? hFont : GetStockObject(SYSTEM_FIXED_FONT));
    GetTextMetrics(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(hdc, old);

    SelectObject(hdc, hFont ? hFont : GetStockObject(SYSTEM_FIXED_FONT));
    SetTextAlign(hdc, TA_RIGHT | TA_TOP);

    int xRight = prc->right - MARGIN;
    int y = MARGIN;
    int i;

    for (i = firstVisible; i < lineCount && y < prc->bottom; i++)
    {
        char num[16];
        sprintf(num, "%d", i + 1);
        TextOutA(hdc, xRight, y, num, (int)strlen(num));
        y += lineHeight;
    }
}

/* ============================================================
   LineNumProc
   Window procedure for the custom line-number gutter control.
   Handles WM_PAINT (delegates to PaintLineNumbers) and
   WM_ERASEBKGND (suppressed to prevent flicker).
   ============================================================ */
LRESULT CALLBACK LineNumProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        PaintLineNumbers(hdc, &rc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* ============================================================
   RegisterLineNumClass
   Registers the "TextorLineNum" window class used for the
   line-number gutter.  Must be called once before WM_CREATE.
   ============================================================ */
BOOL RegisterLineNumClass(void)
{
    WNDCLASSEXA wc = {sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = LineNumProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = "TextorLineNum";
    return RegisterClassExA(&wc) != 0;
}

/* ============================================================
   EditSubclassProc
   Subclasses the built-in EDIT control to intercept editing
   messages:
   - Pushes an undo snapshot BEFORE every destructive keystroke
     (WM_CHAR, VK_DELETE, VK_BACK, WM_PASTE, WM_CUT).
   - Clears the redo stack after any new edit.
   - Updates the window title, line numbers, and status bar
     after every change or cursor movement.
   All unhandled messages are forwarded to the original proc.
   ============================================================ */
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HWND parent = GetParent(hwnd);
    if (!g_bRestoringUndo)
    {
        if (msg == WM_CHAR || msg == WM_PASTE ||
            (msg == WM_KEYDOWN && (wParam == VK_DELETE || wParam == VK_BACK)) ||
            (msg == WM_CUT))
        {
            PushUndo();
        }
    }
    LRESULT ret = CallWindowProc(g_lpfnEditProc, hwnd, msg, wParam, lParam);
    if (!g_bRestoringUndo &&
        (msg == WM_CHAR || msg == WM_PASTE || msg == WM_CUT ||
         (msg == WM_KEYDOWN && (wParam == VK_DELETE || wParam == VK_BACK))))
    {
        ClearRedo();
    }
    switch (msg)
    {
    case WM_CHAR:
        g_bModified = TRUE;
        UpdateWindowTitle(parent);
        UpdateLineNumbers();
        UpdateStatusBar(parent);
        break;
    case WM_KEYDOWN:
        if (wParam == VK_DELETE || wParam == VK_BACK)
            g_bModified = TRUE;
        UpdateWindowTitle(parent);
        UpdateLineNumbers();
        UpdateStatusBar(parent);
        break;
    case WM_LBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_VSCROLL:
    case WM_HSCROLL:
        UpdateLineNumbers();
        UpdateStatusBar(parent);
        break;
    case WM_KEYUP:
        UpdateStatusBar(parent);
        UpdateLineNumbers();
        break;
    }
    return ret;
}

/* ============================================================
   UpdateLineNumbers
   Forces a repaint of the line-number gutter so it stays in
   sync with the edit control's scroll position and content.
   ============================================================ */
void UpdateLineNumbers(void)
{
    if (g_hLineNum && IsWindow(g_hLineNum))
        InvalidateRect(g_hLineNum, NULL, TRUE);
}

/* ============================================================
   UpdateStatusBar
   Reads the current caret position from the edit control and
   updates the status bar text with line number, column, total
   line count, and encoding (always shown as UTF-8).
   ============================================================ */
void UpdateStatusBar(HWND hwnd)
{
    (void)hwnd;
    if (!g_hStatusBar || !IsWindow(g_hStatusBar))
        return;
    int caret = 0;
    SendMessage(g_hEdit, EM_GETSEL, (WPARAM)&caret, (LPARAM)0);
    int line = (int)SendMessage(g_hEdit, EM_LINEFROMCHAR, caret, 0);
    int lineStart = (int)SendMessage(g_hEdit, EM_LINEINDEX, line, 0);
    int col = caret - lineStart;
    int lineCount = (int)SendMessage(g_hEdit, EM_GETLINECOUNT, 0, 0);
    if (lineCount <= 0)
        lineCount = 1;
    char buf[128];
    sprintf(buf, "  Ln %d, Col %d  |  %d line%s  |  UTF-8", line + 1, col + 1, lineCount, lineCount == 1 ? "" : "s");
    SendMessageA(g_hStatusBar, WM_SETTEXT, 0, (LPARAM)buf);
}

/* ============================================================
   WndProc
   Main window procedure.  Key messages handled:

   WM_CREATE      : creates the edit control, gutter, status bar,
                    and initial font; subclasses the edit control.
   WM_CTLCOLOREDIT: colours the edit control background/text.
   WM_CTLCOLORSTATIC: colours the status bar and gutter.
   WM_SIZE        : repositions all child controls on resize.
   WM_SETFOCUS    : forwards focus to the edit control.
   WM_COMMAND     : dispatches all menu/toolbar actions
                    (File, Edit, View, Help).
   WM_CLOSE       : prompts to save before closing.
   WM_DESTROY     : posts WM_QUIT to end the message loop.
   ============================================================ */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int clientH = rc.bottom - rc.top;
        int clientW = rc.right - rc.left;
        int editH = clientH - STATUSBAR_HEIGHT;
        if (editH < 0)
            editH = 0;

        g_hLineNum = CreateWindowExA(WS_EX_STATICEDGE, "TextorLineNum", NULL,
                                     WS_CHILD | WS_VISIBLE,
                                     0, 0, LINENUM_WIDTH, editH,
                                     hwnd, (HMENU)IDC_LINENUM, GetModuleHandle(NULL), NULL);

        g_hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", NULL,
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                      ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                                      ES_NOHIDESEL | ES_WANTRETURN,
                                  LINENUM_WIDTH + MARGIN, 0,
                                  clientW - LINENUM_WIDTH - MARGIN, editH,
                                  hwnd, (HMENU)IDC_EDIT, GetModuleHandle(NULL), NULL);

        g_hStatusBar = CreateWindowExA(0, "STATIC", "  Ln 1, Col 1  |  1 line",
                                       WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                                       0, editH, clientW, STATUSBAR_HEIGHT,
                                       hwnd, (HMENU)IDC_STATUSBAR, GetModuleHandle(NULL), NULL);

        g_hFont = CreateFontA(g_nFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        if (g_hFont)
        {
            SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            SendMessage(g_hLineNum, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        }

        g_lpfnEditProc = (WNDPROC)SetWindowLongPtr(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
        g_hBrushEditBg = CreateSolidBrush(g_clrEditBg);
        g_hBrushLineNumBg = CreateSolidBrush(g_clrLineNumBg);
        ApplyTheme(hwnd);
        UpdateStatusBar(hwnd);
        return 0;
    }

    case WM_CTLCOLOREDIT:
        if ((HWND)lParam == g_hEdit)
        {
            if (!g_hBrushEditBg)
                g_hBrushEditBg = CreateSolidBrush(g_clrEditBg);
            SetBkColor((HDC)wParam, g_clrEditBg);
            SetTextColor((HDC)wParam, g_clrEditText);
            return (LRESULT)g_hBrushEditBg;
        }
        break;

    case WM_CTLCOLORSTATIC:
        if ((HWND)lParam == g_hStatusBar)
        {
            if (!g_hBrushLineNumBg)
                g_hBrushLineNumBg = CreateSolidBrush(g_clrLineNumBg);
            SetBkColor((HDC)wParam, g_clrLineNumBg);
            SetTextColor((HDC)wParam, g_bDarkTheme ? RGB(212, 212, 212) : GetSysColor(COLOR_WINDOWTEXT));
            return (LRESULT)g_hBrushLineNumBg;
        }
        if ((HWND)lParam == g_hLineNum)
        {
            if (!g_hBrushLineNumBg)
                g_hBrushLineNumBg = CreateSolidBrush(g_clrLineNumBg);
            SetBkColor((HDC)wParam, g_clrLineNumBg);
            SetTextColor((HDC)wParam, g_clrLineNumText);
            return (LRESULT)g_hBrushLineNumBg;
        }
        break;

    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        int editH = height - STATUSBAR_HEIGHT;
        if (editH < 0)
            editH = 0;
        MoveWindow(g_hLineNum, 0, 0, LINENUM_WIDTH, editH, TRUE);
        MoveWindow(g_hEdit, LINENUM_WIDTH + MARGIN, 0, width - LINENUM_WIDTH - MARGIN, editH, TRUE);
        MoveWindow(g_hStatusBar, 0, editH, width, STATUSBAR_HEIGHT, TRUE);
        return 0;
    }

    case WM_SETFOCUS:
        SetFocus(g_hEdit);
        return 0;

    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        switch (id)
        {
        case IDM_FILE_NEW:
            if (!PromptSaveIfModified(hwnd))
                break;
            SetWindowText(g_hEdit, "");
            g_szFilePath[0] = '\0';
            strcpy(g_szFileName, "Untitled");
            g_bModified = FALSE;
            UpdateWindowTitle(hwnd);
            ClearUndoRedo();
            SendMessage(g_hEdit, EM_EMPTYUNDOBUFFER, 0, 0);
            UpdateLineNumbers();
            UpdateStatusBar(hwnd);
            break;
        case IDM_FILE_OPEN:
            if (!PromptSaveIfModified(hwnd))
                break;
            {
                OPENFILENAMEA ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
                ofn.lpstrFile = g_szFilePath;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameA(&ofn))
                    DoOpenFile(hwnd, g_szFilePath);
            }
            break;
        case IDM_FILE_SAVE:
            if (g_szFilePath[0])
                DoSaveFile(hwnd, g_szFilePath);
            else
            {
                OPENFILENAMEA ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
                ofn.lpstrFile = g_szFilePath;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT;
                ofn.lpstrDefExt = "txt";
                if (!GetSaveFileNameA(&ofn))
                    break;
                DoSaveFile(hwnd, g_szFilePath);
            }
            break;
        case IDM_FILE_SAVEAS:
        {
            char savePath[MAX_PATH];
            if (g_szFilePath[0])
                strcpy(savePath, g_szFilePath);
            else
                savePath[0] = '\0';
            OPENFILENAMEA ofn = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = savePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = "txt";
            if (!GetSaveFileNameA(&ofn))
                break;
            DoSaveFile(hwnd, savePath);
            break;
        }
        case IDM_FILE_EXIT:
            SendMessage(hwnd, WM_CLOSE, 0, 0);
            break;
        case IDM_EDIT_CUT:
            SendMessage(g_hEdit, WM_CUT, 0, 0);
            break;
        case IDM_EDIT_COPY:
            SendMessage(g_hEdit, WM_COPY, 0, 0);
            break;
        case IDM_EDIT_PASTE:
            SendMessage(g_hEdit, WM_PASTE, 0, 0);
            break;
        case IDM_EDIT_UNDO:
            DoUndo(hwnd);
            break;
        case IDM_EDIT_REDO:
            DoRedo(hwnd);
            break;
        case IDM_EDIT_SELALL:
            SendMessage(g_hEdit, EM_SETSEL, 0, -1);
            UpdateStatusBar(hwnd);
            break;
        case IDM_EDIT_FIND:
            DoFind(hwnd);
            break;
        case IDM_EDIT_REPLACE:
            DoReplace(hwnd);
            break;
        case IDM_EDIT_GOTOLINE:
            DoGoToLine(hwnd);
            break;
        case IDM_EDIT_INDENT:
        {
            int start, end;
            SendMessage(g_hEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
            int line = (int)SendMessage(g_hEdit, EM_LINEFROMCHAR, start, 0);
            int lineStart = (int)SendMessage(g_hEdit, EM_LINEINDEX, line, 0);
            PushUndo();
            SendMessage(g_hEdit, EM_SETSEL, lineStart, lineStart);
            SendMessage(g_hEdit, EM_REPLACESEL, TRUE, (LPARAM) "\t");
            g_bModified = TRUE;
            UpdateWindowTitle(hwnd);
            UpdateLineNumbers();
            UpdateStatusBar(hwnd);
            break;
        }
        case IDM_VIEW_LIGHT:
            g_bDarkTheme = FALSE;
            ApplyTheme(hwnd);
            break;
        case IDM_VIEW_DARK:
            g_bDarkTheme = TRUE;
            ApplyTheme(hwnd);
            break;
        case IDM_VIEW_WORDWRAP:
            ToggleWordWrap(hwnd);
            break;
        case IDM_VIEW_FONT_SMALL:
            g_nFontSize = FONT_SIZE_SMALL;
            ApplyFont(hwnd);
            break;
        case IDM_VIEW_FONT_MEDIUM:
            g_nFontSize = FONT_SIZE_MEDIUM;
            ApplyFont(hwnd);
            break;
        case IDM_VIEW_FONT_LARGE:
            g_nFontSize = FONT_SIZE_LARGE;
            ApplyFont(hwnd);
            break;
        case IDM_VIEW_ZOOM_IN:
            if (g_nFontSize < FONT_SIZE_HUGE)
            {
                g_nFontSize += 2;
                ApplyFont(hwnd);
            }
            break;
        case IDM_VIEW_ZOOM_OUT:
            if (g_nFontSize > FONT_SIZE_TINY)
            {
                g_nFontSize -= 2;
                ApplyFont(hwnd);
            }
            break;
        case IDM_HELP_ABOUT:
            MessageBoxA(hwnd,
                        "Textor - A Windows 32-bit Text & Code Editor\n\n"
                        "Developed in pure C using Windows API.\n\n"
                        "Features: Multi-level Undo/Redo, Find & Replace,\n"
                        "Go to Line, Dark/Light Theme, Word Wrap,\n"
                        "Zoom In/Out, Line Numbers, Status Bar.",
                        "About Textor", MB_OK | MB_ICONINFORMATION);
            break;
        }
        return 0;
    }

    case WM_CLOSE:
        if (!PromptSaveIfModified(hwnd))
            return 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* ============================================================
   CreateAccelerators
   Builds and returns the keyboard accelerator table that maps
   common Ctrl+key shortcuts to their menu command IDs.
   ============================================================ */
static HACCEL CreateAccelerators(void)
{
    ACCEL acc[] = {
        {FVIRTKEY | FCONTROL, 'N', IDM_FILE_NEW},
        {FVIRTKEY | FCONTROL, 'O', IDM_FILE_OPEN},
        {FVIRTKEY | FCONTROL, 'S', IDM_FILE_SAVE},
        {FVIRTKEY | FCONTROL, 'X', IDM_EDIT_CUT},
        {FVIRTKEY | FCONTROL, 'C', IDM_EDIT_COPY},
        {FVIRTKEY | FCONTROL, 'V', IDM_EDIT_PASTE},
        {FVIRTKEY | FCONTROL, 'Z', IDM_EDIT_UNDO},
        {FVIRTKEY | FCONTROL, 'Y', IDM_EDIT_REDO},
        {FVIRTKEY | FCONTROL, 'A', IDM_EDIT_SELALL},
        {FVIRTKEY | FCONTROL, 'F', IDM_EDIT_FIND},
        {FVIRTKEY | FCONTROL, 'H', IDM_EDIT_REPLACE},
        {FVIRTKEY | FCONTROL, 'G', IDM_EDIT_GOTOLINE},
        {FVIRTKEY | FCONTROL, VK_OEM_PLUS, IDM_VIEW_ZOOM_IN},
        {FVIRTKEY | FCONTROL | FSHIFT, '=', IDM_VIEW_ZOOM_IN},
        {FVIRTKEY | FCONTROL, VK_OEM_MINUS, IDM_VIEW_ZOOM_OUT},
    };
    return CreateAcceleratorTableA(acc, sizeof(acc) / sizeof(acc[0]));
}

/* ============================================================
   WinMain  -  Application entry point
   1. Registers the line-number gutter class.
   2. Creates the accelerator table.
   3. Registers and creates the main window.
   4. Attaches the menu and sets the default font-size check mark.
   5. If a file path was passed on the command line, opens it.
   6. Runs the standard Win32 message loop, translating
      accelerator keys before dispatching messages.
   ============================================================ */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;

    if (!RegisterLineNumClass())
        return 1;

    HACCEL hAccel = CreateAccelerators();

    WNDCLASSEXA wc = {sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "TextorMain";
    if (!RegisterClassExA(&wc))
        return 1;

    g_hWndMain = CreateWindowExA(0, "TextorMain", "Untitled - Textor",
                                 WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                                 NULL, NULL, hInstance, NULL);
    if (!g_hWndMain)
        return 1;

    SetMenu(g_hWndMain, CreateMainMenu());

    HMENU hMenu = GetMenu(g_hWndMain);
    if (hMenu)
    {
        HMENU hView = GetSubMenu(hMenu, 2);
        if (hView)
            CheckMenuItem(hView, IDM_VIEW_FONT_MEDIUM, MF_BYCOMMAND | MF_CHECKED);
    }

    if (lpCmdLine && *lpCmdLine)
    {
        while (*lpCmdLine == ' ')
            lpCmdLine++;
        if (*lpCmdLine == '"')
        {
            lpCmdLine++;
            char cmdPathBuf[MAX_PATH] = "";
            size_t n = 0;
            while (*lpCmdLine && *lpCmdLine != '"' && n < MAX_PATH - 1)
                cmdPathBuf[n++] = *lpCmdLine++;
            cmdPathBuf[n] = '\0';
            if (n > 0)
                DoOpenFile(g_hWndMain, cmdPathBuf);
        }
        else if (strlen(lpCmdLine) < MAX_PATH)
        {
            DoOpenFile(g_hWndMain, lpCmdLine);
        }
    }

    ShowWindow(g_hWndMain, nCmdShow);
    UpdateWindow(g_hWndMain);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(g_hWndMain, hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    DestroyAcceleratorTable(hAccel);
    return (int)msg.wParam;
}