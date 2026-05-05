/* File: readdib.h */

/*
 * This file has been modified for use with "Angband 2.8.2"
 *
 * Copyright 1991 Microsoft Corporation. All rights reserved.
 */

/*
 * Information about a bitmap
 */
typedef struct
{
    HANDLE hDIB;
    HBITMAP hBitmap;
    HPALETTE hPalette;
    BYTE CellWidth;
    BYTE CellHeight;
} DIBINIT;

/// Read a DIB from a file. If `tint_rage` is TRUE, the rage tint shader is
/// applied to every color before the device-dependent bitmap is created.
extern BOOL ReadDIB(HWND, LPSTR, DIBINIT*, BOOL tint_rage);

/// Convenience wrapper around ReadDIB that reads a DIB file as-is.
extern BOOL ReadDIBNormal(HWND, LPSTR, DIBINIT*);
/// Convenience wrapper around ReadDIB that applies a red tint to a DIB file.
extern BOOL ReadDIBRage(HWND, LPSTR, DIBINIT*);

/* Free a DIB */
extern void FreeDIB(DIBINIT* dib);
