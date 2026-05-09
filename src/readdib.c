/* File: readdib.c */

/*
 * This package provides a routine to read a DIB file and set up the
 * device dependent version of the image.
 *
 * This file has been modified for use with "Angband 2.9.2"
 *
 * COPYRIGHT:
 *
 *   (C) Copyright Microsoft Corp. 1993.  All rights reserved.
 *
 *   You have a royalty-free right to use, modify, reproduce and
 *   distribute the Sample Files (and/or any modified version) in
 *   any way you find useful, provided that you agree that
 *   Microsoft has no warranty obligations or liability for any
 *   Sample Application Files which are modified.
 */

#include <windows.h>

#include "angband.h"
#include "readdib.h"

/// Apply the rage-tint transform to an RGB triple in-place.
///
/// This is used to apply a red tint to tiles to change the visuals when the
/// player character is raging.
///
/// Sil-Q has the convention that pixel (0,0) in the BMP tileset defines the
/// transparent color. The BMP format does not support transparency natively, so
/// `blank_r/g/b` is discovered from pixel (0,0) at load time and treated as
/// BMP's transparent color. The tint preserves this exact RGB so that
/// compositing-time transparency continues to work, regardless of what RGB
/// value was chosen for it when creating the BMP tileset.
///
/// For all other pixels: convert to luma and rescale to (luma * R, luma * G,
/// luma * B) using the RAGE_TINT_*_COEFF defines.
static void tint_rgb_for_rage(
    BYTE* r, BYTE* g, BYTE* b, BYTE blank_r, BYTE blank_g, BYTE blank_b)
{
    if (*r == blank_r && *g == blank_g && *b == blank_b)
        return;

    int luma = (299 * (int)*r + 587 * (int)*g + 114 * (int)*b) / 1000;
    if (luma > 255)
        luma = 255;

    int nr = (int)(luma * RAGE_TINT_RED_COEFF);
    int ng = (int)(luma * RAGE_TINT_GREEN_COEFF);
    int nb = (int)(luma * RAGE_TINT_BLUE_COEFF);
    if (nr > 255)
        nr = 255;
    if (ng > 255)
        ng = 255;
    if (nb > 255)
        nb = 255;

    *r = (BYTE)nr;
    *g = (BYTE)ng;
    *b = (BYTE)nb;
}

/// Walk the loaded DIB in memory and apply the rage tint to every color.
///
/// For 8-bit (or smaller) indexed BMPs, only the color table is tinted. The
/// pixel data is just indices into that table, so tinting the palette is
/// sufficient.
///
/// For 24-bit BMPs, both the (default) color table and every pixel triple are
/// tinted in-place.
///
/// The shader's blank-color guard tracks whatever the artist chose for
/// pixel (0,0), discovered from the loaded DIB. The rage tileset stays
/// consistent with the compositor's transparency check regardless of what
/// the BMP magic color is.
///
/// The DIB must be locked before this is called.
static void tint_dib_in_place(LPBITMAPINFOHEADER lpInfo)
{
    DWORD i;
    RGBQUAD FAR* lpRGB = (RGBQUAD FAR*)((LPSTR)lpInfo + lpInfo->biSize);
    LPSTR lpBits = ((LPSTR)lpInfo + lpInfo->biSize
        + lpInfo->biClrUsed * sizeof(RGBQUAD));

    /*
     * Discover the blank color for transparency from pixel (0,0). Image pixel
     * (0,0) is the top-left visual pixel; in BMP storage that's the LAST file
     * row because BMP is bottom-up.
     * For 8-bit BMPs we fseek and read one palette-index byte. For 24-bit
     * BMPs we read three bytes (B, G, R) directly as the blank RGB, since
     * there's no palette indirection.
     */
    int blank_index = -1;
    BYTE blank_r = 0, blank_g = 0, blank_b = 0;
    LONG bytesPerRow
        = (((LONG)lpInfo->biWidth * lpInfo->biBitCount + 31) / 32) * 4;
    LONG pixel00_offset = (LONG)(lpInfo->biHeight - 1) * bytesPerRow;
    if (lpInfo->biBitCount == 8)
    {
        BYTE idx = ((BYTE*)lpBits)[pixel00_offset];
        blank_index = (int)idx;
        blank_r = lpRGB[idx].rgbRed;
        blank_g = lpRGB[idx].rgbGreen;
        blank_b = lpRGB[idx].rgbBlue;
    }
    else if (lpInfo->biBitCount == 24)
    {
        BYTE* p = (BYTE*)lpBits + pixel00_offset;
        blank_b = p[0];
        blank_g = p[1];
        blank_r = p[2];
    }

    /* Tint the color table (skipping the blank index). */
    for (i = 0; i < lpInfo->biClrUsed; i++)
    {
        if ((int)i == blank_index)
            continue;
        tint_rgb_for_rage(&lpRGB[i].rgbRed, &lpRGB[i].rgbGreen,
            &lpRGB[i].rgbBlue, blank_r, blank_g, blank_b);
    }

    /* For 24-bit BMPs the pixels are direct RGB triples; tint them too. */
    if (lpInfo->biBitCount == 24)
    {
        LONG row, col;
        for (row = 0; row < lpInfo->biHeight; row++)
        {
            BYTE* p = (BYTE*)(lpBits + row * bytesPerRow);
            for (col = 0; col < lpInfo->biWidth; col++)
            {
                /* BMP stores B, G, R per pixel. */
                tint_rgb_for_rage(
                    &p[2], &p[1], &p[0], blank_r, blank_g, blank_b);
                p += 3;
            }
        }
    }
}

/*
 * Extract the "WIN32" flag from the compiler
 */
#if defined(_WIN32)
#ifndef WIN32
#define WIN32
#endif
#endif

/*
 * Needed for lcc-win32
 */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif

/*
 * Number of bytes to be read during each read operation
 */
#define MAXREAD 32768

/*
 * Private routine to read more than 64K at a time
 *
 * Reads data in steps of 32k till all the data has been read.
 *
 * Returns number of bytes requested, or zero if something went wrong.
 */
static DWORD PASCAL lread(HFILE fh, VOID FAR* pv, DWORD ul)
{
    DWORD ulT = ul;
    BYTE* hp = pv;

    while (ul > (DWORD)MAXREAD)
    {
        if (_lread(fh, (LPSTR)hp, (WORD)MAXREAD) != MAXREAD)
            return 0;
        ul -= MAXREAD;
        hp += MAXREAD;
    }
    if (_lread(fh, (LPSTR)hp, (WORD)ul) != (WORD)ul)
        return 0;
    return ulT;
}

/*
 * Given a BITMAPINFOHEADER, create a palette based on the color table.
 *
 * Returns the handle of a palette, or zero if something went wrong.
 */
static HPALETTE PASCAL NEAR MakeDIBPalette(LPBITMAPINFOHEADER lpInfo)
{
    PLOGPALETTE npPal;
    RGBQUAD FAR* lpRGB;
    HPALETTE hLogPal;
    DWORD i;

    /*
     * since biClrUsed field was filled during the loading of the DIB,
     * we know it contains the number of colors in the color table.
     */
    if (lpInfo->biClrUsed)
    {
        npPal = (PLOGPALETTE)LocalAlloc(LMEM_FIXED,
            sizeof(LOGPALETTE)
                + (WORD)lpInfo->biClrUsed * sizeof(PALETTEENTRY));
        if (!npPal)
            return (FALSE);

        npPal->palVersion = 0x300;
        npPal->palNumEntries = (WORD)lpInfo->biClrUsed;

        /* get pointer to the color table */
        lpRGB = (RGBQUAD FAR*)((LPSTR)lpInfo + lpInfo->biSize);

        /* copy colors from the color table to the LogPalette structure */
        for (i = 0; i < lpInfo->biClrUsed; i++, lpRGB++)
        {
            npPal->palPalEntry[i].peRed = lpRGB->rgbRed;
            npPal->palPalEntry[i].peGreen = lpRGB->rgbGreen;
            npPal->palPalEntry[i].peBlue = lpRGB->rgbBlue;
            npPal->palPalEntry[i].peFlags = PC_NOCOLLAPSE;
        }

        hLogPal = CreatePalette((LPLOGPALETTE)npPal);
        LocalFree((HANDLE)npPal);
        return (hLogPal);
    }

    /*
     * 24-bit DIB with no color table.  return default palette.  Another
     * option would be to create a 256 color "rainbow" palette to provide
     * some good color choices.
     */
    else
    {
        return (GetStockObject(DEFAULT_PALETTE));
    }
}

/*
 * Given a DIB, create a bitmap and corresponding palette to be used for a
 * device-dependent representation of the image.
 *
 * Returns TRUE on success (phPal and phBitmap are filled with appropriate
 * handles.  Caller is responsible for freeing objects) and FALSE on failure
 * (unable to create objects, both pointer are invalid).
 */
static BOOL NEAR PASCAL MakeBitmapAndPalette(
    HDC hDC, HANDLE hDIB, HPALETTE* phPal, HBITMAP* phBitmap)
{
    LPBITMAPINFOHEADER lpInfo;
    BOOL result = FALSE;
    HBITMAP hBitmap;
    HPALETTE hPalette, hOldPal;
    LPSTR lpBits;

    lpInfo = (LPBITMAPINFOHEADER)GlobalLock(hDIB);
    if ((hPalette = MakeDIBPalette(lpInfo)) != 0)
    {
        /* Need to realize palette for converting DIB to bitmap. */
        hOldPal = SelectPalette(hDC, hPalette, TRUE);
        RealizePalette(hDC);

        lpBits = ((LPSTR)lpInfo + (WORD)lpInfo->biSize
            + (WORD)lpInfo->biClrUsed * sizeof(RGBQUAD));
        hBitmap = CreateDIBitmap(hDC, lpInfo, CBM_INIT, lpBits,
            (LPBITMAPINFO)lpInfo, DIB_RGB_COLORS);

        SelectPalette(hDC, hOldPal, TRUE);
        RealizePalette(hDC);

        if (!hBitmap)
        {
            DeleteObject(hPalette);
        }
        else
        {
            *phBitmap = hBitmap;
            *phPal = hPalette;
            result = TRUE;
        }
    }
    return (result);
}

/// Reads a DIB from a file, obtains a handle to its BITMAPINFO struct, and
/// loads the DIB.  Once the DIB is loaded, the function also creates a bitmap
/// and palette out of the DIB for a device-dependent form.
///
/// If `tint_rage` is TRUE, every color in the DIB (palette entries and, for
/// 24-bit BMPs, every pixel triple) is rewritten via `tint_rgb_for_rage()`
/// before the device-dependent bitmap is created. The returned bitmap is
/// therefore the rage-tinted variant. Used by the rage tint shader to build a
/// second tileset that the renderer switches to while the player is raging.
///
/// Returns TRUE if the DIB is loaded and the bitmap/palette created, in which
/// case, the DIBINIT structure pointed to by pInfo is filled with the
/// appropriate handles, and FALSE if something went wrong.
BOOL ReadDIB(HWND hWnd, LPSTR lpFileName, DIBINIT* pInfo, BOOL tint_rage)
{
    HFILE fh;
    LPBITMAPINFOHEADER lpbi;
    OFSTRUCT of;
    BITMAPFILEHEADER bf;
    WORD nNumColors;
    BOOL result = FALSE;
    DWORD offBits;
    HDC hDC;
    BOOL bCoreHead = FALSE;

    /* Open the file and get a handle to it's BITMAPINFO */
    fh = OpenFile(lpFileName, &of, OF_READ);
    if (fh == -1)
        return (FALSE);

    pInfo->hDIB = GlobalAlloc(
        GHND, (DWORD)(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)));

    if (!pInfo->hDIB)
        return (FALSE);

    lpbi = (LPBITMAPINFOHEADER)GlobalLock(pInfo->hDIB);

    /* read the BITMAPFILEHEADER */
    if (sizeof(bf) != _lread(fh, (LPSTR)&bf, sizeof(bf)))
        goto ErrExit;

    /* 'BM' */
    if (bf.bfType != 0x4d42)
        goto ErrExit;

    if (sizeof(BITMAPCOREHEADER)
        != _lread(fh, (LPSTR)lpbi, sizeof(BITMAPCOREHEADER)))
        goto ErrExit;

    if (lpbi->biSize == sizeof(BITMAPCOREHEADER))
    {
        lpbi->biSize = sizeof(BITMAPINFOHEADER);
        lpbi->biBitCount = ((LPBITMAPCOREHEADER)lpbi)->bcBitCount;
        lpbi->biPlanes = ((LPBITMAPCOREHEADER)lpbi)->bcPlanes;
        lpbi->biHeight = ((LPBITMAPCOREHEADER)lpbi)->bcHeight;
        lpbi->biWidth = ((LPBITMAPCOREHEADER)lpbi)->bcWidth;
        bCoreHead = TRUE;
    }
    else
    {
        /* get to the start of the header and read INFOHEADER */
        _llseek(fh, sizeof(BITMAPFILEHEADER), SEEK_SET);
        if (sizeof(BITMAPINFOHEADER)
            != _lread(fh, (LPSTR)lpbi, sizeof(BITMAPINFOHEADER)))
            goto ErrExit;
    }

    if (!(nNumColors = (WORD)lpbi->biClrUsed))
    {
        /* no color table for 24-bit, default size otherwise */
        if (lpbi->biBitCount != 24)
            nNumColors = 1 << lpbi->biBitCount;
    }

    /* fill in some default values if they are zero */
    if (lpbi->biClrUsed == 0)
        lpbi->biClrUsed = nNumColors;

    if (lpbi->biSizeImage == 0)
    {
        lpbi->biSizeImage
            = (((((lpbi->biWidth * (DWORD)lpbi->biBitCount) + 31) & ~31) >> 3)
                * lpbi->biHeight);
    }

    /* otherwise wouldn't work with 16 color bitmaps -- S.K. */
    else if ((nNumColors == 16) && (lpbi->biSizeImage > bf.bfSize))
    {
        lpbi->biSizeImage /= 2;
    }

    /* get a proper-sized buffer for header, color table and bits */
    GlobalUnlock(pInfo->hDIB);
    pInfo->hDIB = GlobalReAlloc(pInfo->hDIB,
        lpbi->biSize + nNumColors * sizeof(RGBQUAD) + lpbi->biSizeImage, 0);

    /* can't resize buffer for loading */
    if (!pInfo->hDIB)
        goto ErrExit2;

    lpbi = (LPBITMAPINFOHEADER)GlobalLock(pInfo->hDIB);

    /* read the color table */
    if (!bCoreHead)
    {
        _lread(fh, (LPSTR)(lpbi) + lpbi->biSize, nNumColors * sizeof(RGBQUAD));
    }
    else
    {
        signed int i;
        RGBQUAD FAR* pQuad;
        RGBTRIPLE FAR* pTriple;

        _lread(
            fh, (LPSTR)(lpbi) + lpbi->biSize, nNumColors * sizeof(RGBTRIPLE));

        pQuad = (RGBQUAD FAR*)((LPSTR)lpbi + lpbi->biSize);
        pTriple = (RGBTRIPLE FAR*)pQuad;
        for (i = nNumColors - 1; i >= 0; i--)
        {
            pQuad[i].rgbRed = pTriple[i].rgbtRed;
            pQuad[i].rgbBlue = pTriple[i].rgbtBlue;
            pQuad[i].rgbGreen = pTriple[i].rgbtGreen;
            pQuad[i].rgbReserved = 0;
        }
    }

    /* offset to the bits from start of DIB header */
    offBits = lpbi->biSize + nNumColors * sizeof(RGBQUAD);

    if (bf.bfOffBits != 0L)
    {
        _llseek(fh, bf.bfOffBits, SEEK_SET);
    }

    /* Use local version of '_lread()' above */
    if (lpbi->biSizeImage
        == lread(fh, (LPSTR)lpbi + offBits, lpbi->biSizeImage))
    {
        // Rage tint shader: rewrite colors before the device-dependent bitmap
        // is created.
        if (tint_rage)
            tint_dib_in_place(lpbi);

        GlobalUnlock(pInfo->hDIB);

        hDC = GetDC(hWnd);
        if (!MakeBitmapAndPalette(
                hDC, pInfo->hDIB, &(pInfo->hPalette), &(pInfo->hBitmap)))
        {
            ReleaseDC(hWnd, hDC);
            goto ErrExit2;
        }
        else
        {
            ReleaseDC(hWnd, hDC);
            result = TRUE;
        }
    }
    else
    {
    ErrExit:
        GlobalUnlock(pInfo->hDIB);
    ErrExit2:
        GlobalFree(pInfo->hDIB);
    }

    _lclose(fh);
    return (result);
}

BOOL ReadDIBNormal(HWND hWnd, LPSTR lpFileName, DIBINIT* pInfo)
{
    return ReadDIB(hWnd, lpFileName, pInfo, FALSE);
}

BOOL ReadDIBRage(HWND hWnd, LPSTR lpFileName, DIBINIT* pInfo)
{
    return ReadDIB(hWnd, lpFileName, pInfo, TRUE);
}

/* Free a DIB */
void FreeDIB(DIBINIT* dib)
{
    /* Free the bitmap stuff */
    if (dib->hPalette)
        DeleteObject(dib->hPalette);
    if (dib->hBitmap)
        DeleteObject(dib->hBitmap);
    if (dib->hDIB)
        GlobalFree(dib->hDIB);

    dib->hPalette = NULL;
    dib->hBitmap = NULL;
    dib->hDIB = NULL;
}
