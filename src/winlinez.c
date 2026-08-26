/* winlinez.c - Color Linez v1.21, reconstructed from WINLINEZ.EXE.
 *
 * Original:  Win32 port (c) 1998-1999 Ivan Golubev <m53group@piter.net>
 *            of the 1992 DOS game "Color Lines" (c) GAMOS LTD,
 *            programmed by Olga Demina, graphics by Igor Ivkin &
 *            Gennady Denisov.
 *
 * Recovered by static analysis (Ghidra) of the original 60KB binary.
 * Original addresses are quoted as FUN_004xxxxx for cross-checking.
 *
 * Build (32-bit mingw):  i686-w64-mingw32-gcc -o winlinez.exe *.c -mwindows
 */

#include <windows.h>
#include <string.h>
#include "resource.h"
#include "gfxdata.h"
#include "inflate.h"

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define APP_CLASS     TEXT("LinezWindow")          /* 0x420020 */
#define APP_TITLE     TEXT("Color linez")          /* 0x420090 */

#define BOARD_W       9
#define BOARD_H       9
#define CELL          36                     /* 0x24 */
#define BOARD_PIX     (BOARD_W * CELL)       /* 324 = 0x148 */

#define WIN_W         620                    /* 0x26C */
#define WIN_H         420                    /* 0x1A4 */
#define TOPBAR_H      43                     /* 0x2B */

#define BALL_EMPTY    0
#define BALL_COLORS   7                      /* colours 1..7 (value 8 is
                                                reserved as the BFS seed
                                                marker - original design) */
#define BALL_MAX      8                      /* stats: empty + 7 colours */
#define MARK_FLAG     0x40                   /* "in line" marker  */
#define SEED_BASE     8                      /* BFS distance base */

#define TIMER_MS      50

#define BAR_H         125                    /* 0x7D progress bar  */
#define HISCORES      10
#define NAME_LEN      13

/* sprite sheet geometry (source rects inside the 511x585 BMP) */
#define SPR_CELL_Y    225                    /* 0xE1 ball cells, rows 0..9  */
#define SPR_NEXT_Y    441                    /* 0x1B9 next-ball previews    */
#define SPR_BUBBLE_Y  201                    /* 0xC9 bubble top cap         */
#define SPR_PORTR_Y   0                      /* red king portraits 73x100   */
#define SPR_PRETND_Y  100                    /* purple king + crowns        */
#define SPR_KING_W    73                     /* 0x49 */
#define SPR_CROWN_W   51                     /* 0x33 */
#define SPR_CROWN_H   65                     /* 0x41 */
#define SPR_CROWN_X   23                     /* 0x17 */
#define SPR_BAR_X     420                    /* 0x1A4 45x100 gradient bar   */
#define SPR_BAR_W     45                     /* 0x2D */

/* ------------------------------------------------------------------ */
/*  Game state (original .bss addresses in comments)                   */
/* ------------------------------------------------------------------ */

static HINSTANCE  g_hInst;                /* 0x430028 */
static HWND       g_hMainWnd;             /* 0x43023C */
static HWND       g_hStatWnd;             /* 0x430234 */
static BOOL       g_statVisible;          /* 0x430238 */
static HACCEL     g_hAccel;               /* 0x430034 */
static int        g_screenW, g_screenH;   /* 0x43002C / 0x430030 */

static HBITMAP    g_hSprites;             /* 0x430038 */
static HPALETTE   g_hPalette;             /* 0x43003C */
static HDC        g_hMemDC;               /* 0x430230 */
static HGDIOBJ    g_hOldBmp;              /* 0x43022C */
static int        g_sprW, g_sprH;         /* 0x430004 / 0x430008 */

static BYTE       g_board[BOARD_H][BOARD_W];   /* 0x430040 [y][x]     */
static BYTE       g_pathY[96], g_pathX[96];    /* 0x430092.. BFS path */
static int        g_pathLen;                   /* 0x430136            */

static int        g_boardX, g_boardY;     /* 0x430138 / 0x43013C      */
static BYTE       g_selX, g_selY;         /* 0x430141 / 0x430142      */
static int        g_score;                /* 0x430144                 */
static int        g_scoreX;               /* 0x430148 clientW-96      */
static int        g_scoreY;               /* 0x43014C = 10            */
static BYTE       g_next[3];              /* 0x430150 next-ball queue */
static int        g_nextX, g_nextY;       /* 0x430154 / 0x430158      */
static BOOL       g_showNext;             /* 0x43015C                 */
static DWORD      g_target;               /* 0x430160 king's score    */
static int        g_barX, g_barY;         /* 0x430164 / 0x430168      */
static int        g_myBarH;               /* 0x43016C player bar fill */
static int        g_myFrame;              /* 0x430170 red king frame  */
static int        g_bar2X, g_bar2Y;       /* 0x430174 / 0x430178      */
static int        g_kingBarH;             /* 0x43017C king bar fill   */
static int        g_crownFrame;           /* 0x430180 crown anim      */
static BOOL       g_kingDown;             /* 0x430184 king defeated   */

static TCHAR      g_myName[16];           /* 0x42002C player label    */           /* 0x42002C player label    */

static char       g_hiscName[HISCORES][NAME_LEN + 1];  /* 0x430188    */
static WORD       g_hiscScore[HISCORES];               /* 0x430196    */
static BOOL       g_hiscDirty;            /* 0x430228                 */

static int        g_colorStat[BALL_MAX];  /* 0x430240 per-colour dels */

static int        g_tick;                 /* 0x43000C                 */
static int        g_state;                /* 0x430010 0=idle 1=selected */
static BYTE       g_moveColor;            /* 0x430018 ball in transit */
static int        g_curX, g_curY;         /* 0x43001C / 0x430020      */
static BOOL       g_downOutside;          /* 0x430024                 */

static DWORD      g_rngSeed;              /* 0x430000 LCG state       */

/* ------------------------------------------------------------------ */
/*  Forward declarations                                               */
/* ------------------------------------------------------------------ */

static void DrawCell(HDC hdc, int x, int y, int frame);
static void DrawScoreBox(HDC hdc, int x, int y, int value);
static void DrawNext(HDC hdc);
static void DrawPlayerBar(HDC hdc, BOOL drawLabel);
static void DrawKingBar(HDC hdc, BOOL drawLabel, BOOL drawKing);
static void NewGame(HDC hdc);
static void ResetKings(HDC hdc);
void        SaveHiscores(void);
static void CheckHiscore(HWND hwnd);
static INT_PTR CALLBACK NameDlgProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK ScoresDlgProc(HWND, UINT, WPARAM, LPARAM);

/* ------------------------------------------------------------------ */
/*  Mini runtime (the original shipped without the CRT)                */
/* ------------------------------------------------------------------ */

/* FUN_00410141: LCG used by the original instead of the CRT rand() */
static int Rand(int n)
{
    g_rngSeed = g_rngSeed * 0x41C64E6D + 0x3039;
    return (int)((g_rngSeed >> 16) & 0x7FFF) % n;
}

/* FUN_00410171 */
static void Srand(DWORD seed) { g_rngSeed = seed; }

/* FUN_004100A8 */
static void Itoa(int val, TCHAR *buf)
{
    TCHAR tmp[16];
    int   div = 1000000000, i = 0, started = 0;
    if (val == 0) { buf[0] = TEXT('0'); buf[1] = TEXT('\0'); return; }
    while (div > 0) {
        int d = val / div;
        val %= div;
        if (d != 0 || started) { tmp[i++] = (TCHAR)(TEXT('0') + d); started = 1; }
        div /= 10;
    }
    tmp[i] = TEXT('\0');
    lstrcpy(buf, tmp);
}

/* hiscore records are stored as ANSI bytes in winlines.res; convert
 * between the file charset and the display charset */
static void AcpToT(const char *src, TCHAR *dst, int dstMax)
{
#ifdef UNICODE
    MultiByteToWideChar(CP_ACP, 0, src, -1, dst, dstMax);
#else
    lstrcpyA(dst, src);
#endif
}
static void TToAcp(const TCHAR *src, char *dst, int dstMax)
{
#ifdef UNICODE
    WideCharToMultiByte(CP_ACP, 0, src, -1, dst, dstMax, NULL, NULL);
#else
    lstrcpyA(dst, src);
#endif
}

/* ------------------------------------------------------------------ */
/*  Sprite loading (FUN_00412A50)                                      */
/* ------------------------------------------------------------------ */

/* The graphics are stored as a raw-deflate compressed 511x585 8bpp BMP
 * inside .data (0x420108, see gfxdata.c).  The original inflates it
 * into a 0x4A000 buffer and feeds ptr+14 to CreateDIBitmap - i.e. the
 * payload is a full BITMAPFILEHEADER + DIB. */
static BOOL LoadSprites(HDC hdc)
{
    BYTE  *raw;
    size_t rawLen = 0;
    DWORD  offBits;
    BITMAPINFOHEADER *bih;
    HGLOBAL hg;

    hg = GlobalAlloc(GMEM_FIXED, 0x4A000);
    if (!hg) return FALSE;
    raw = (BYTE *)hg;

    if (inflate_raw(g_packedGfx, g_packedGfxSize, raw, 0x4A000, &rawLen))
        goto fail;
    if (rawLen < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) ||
        raw[0] != 'B' || raw[1] != 'M')
        goto fail;

    offBits = *(DWORD *)(raw + 10);
    bih     = (BITMAPINFOHEADER *)(raw + 14);
    g_sprW  = bih->biWidth;
    g_sprH  = bih->biHeight;

    if (bih->biBitCount <= 8) {
        int         nColors = bih->biClrUsed ? (int)bih->biClrUsed
                                             : (1 << bih->biBitCount);
        RGBQUAD    *src = (RGBQUAD *)((BYTE *)bih + bih->biSize);
        LOGPALETTE *lp = (LOGPALETTE *)GlobalAlloc(GMEM_FIXED,
                            sizeof(LOGPALETTE) + nColors * sizeof(PALETTEENTRY));
        if (lp) {
            int i;
            lp->palVersion    = 0x300;
            lp->palNumEntries = (WORD)nColors;
            for (i = 0; i < nColors; i++) {
                lp->palPalEntry[i].peRed   = src[i].rgbRed;
                lp->palPalEntry[i].peGreen = src[i].rgbGreen;
                lp->palPalEntry[i].peBlue  = src[i].rgbBlue;
                lp->palPalEntry[i].peFlags = 0;
            }
            g_hPalette = CreatePalette(lp);
            GlobalFree((HGLOBAL)lp);
        }
    }

    g_hSprites = CreateDIBitmap(hdc, bih, CBM_INIT,
                                raw + offBits, (BITMAPINFO *)bih, DIB_RGB_COLORS);
    GlobalFree(hg);
    return g_hSprites != NULL;

fail:
    GlobalFree(hg);
    return FALSE;
}

/* ------------------------------------------------------------------ */
/*  Drawing                                                            */
/* ------------------------------------------------------------------ */

/* FUN_0041042D - one board cell; frame = sprite row (0..5 roll/blink,
 * 6..7 birth, 8..9 explosion).  MARK_FLAG never affects the sprite. */
static void DrawCell(HDC hdc, int x, int y, int frame)
{
    BitBlt(hdc, x * CELL + g_boardX + 2, y * CELL + g_boardY + 2,
           CELL, CELL, g_hMemDC,
           (g_board[y][x] & 0x3F) * CELL, frame * CELL + SPR_CELL_Y, SRCCOPY);
}

/* FUN_004102F9 - 3D grid frame + every cell */
static void DrawBoard(HDC hdc)
{
    HPEN   hPen, hOld;
    int    x, y;

    hPen = CreatePen(PS_SOLID, 1, RGB(0x55, 0x55, 0x55));
    hOld = SelectObject(hdc, hPen);
    MoveToEx(hdc, g_boardX, g_boardY + BOARD_PIX - 1, NULL);
    LineTo(hdc, g_boardX + BOARD_PIX - 1, g_boardY + BOARD_PIX - 1);
    LineTo(hdc, g_boardX + BOARD_PIX - 1, g_boardY);
    SelectObject(hdc, GetStockObject(WHITE_PEN));
    LineTo(hdc, g_boardX, g_boardY);
    LineTo(hdc, g_boardX, g_boardY + BOARD_PIX);
    SelectObject(hdc, hOld);
    DeleteObject(hPen);

    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++)
            DrawCell(hdc, x, y, 0);
}

/* FUN_00410BE4 - two-edge 3D frame helper */
static void DrawBox3D(HDC hdc, int y1, int x1, int x2, int y2,
                      HPEN penA, HPEN penB)
{
    HGDIOBJ hOld = SelectObject(hdc, penA);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x1, y2);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, penB);
    MoveToEx(hdc, x1 + 1, y1, NULL);
    LineTo(hdc, x2, y1);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, hOld);
}

/* FUN_00410D72 - black score display with double 3D frame */
static void DrawScoreBox(HDC hdc, int x, int y, int value)
{
    RECT   rc;
    HPEN   hLight, hShadow;
    TCHAR  buf[16];

    rc.left  = x + 4;  rc.right  = x + 0x44;
    rc.top   = y + 4;  rc.bottom = y + 0x14;
    FillRect(hdc, &rc, GetStockObject(BLACK_BRUSH));

    hLight  = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHIGHLIGHT));
    hShadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));

    /* outer raised frame (x,y)-(x+0x47,y+0x17)  (0x410DE8):
     * highlight left+top, shadow bottom+right */
    DrawBox3D(hdc, y + 0x17, x, x + 0x47, y, hLight, hShadow);
    /* inner sunken frame (x+3,y+3)-(x+0x44,y+0x14) (0x410E09):
     * shadow left+top, highlight bottom+right */
    DrawBox3D(hdc, y + 0x14, x + 3, x + 0x44, y + 3, hShadow, hLight);
    DeleteObject(hLight);
    DeleteObject(hShadow);

    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    Itoa(value, buf);
    rc.right -= 8;                       /* 0x410E49: text right inset */
    DrawText(hdc, buf, -1, &rc, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
}

/* FUN_00410E5A - gray top bar with embossed captions */
static void DrawTopBar(HWND hwnd, HDC hdc)
{
    RECT   rc;
    HBRUSH hbr;
    HPEN   hLight, hShadow, hOld;

    GetClientRect(hwnd, &rc);
    rc.bottom = rc.top + TOPBAR_H;
    hbr = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    FillRect(hdc, &rc, hbr);
    DeleteObject(hbr);

    hLight  = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHIGHLIGHT));
    hShadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
    hOld    = SelectObject(hdc, hShadow);
    MoveToEx(hdc, 0, 0, NULL);          LineTo(hdc, rc.right + 1, 0);
    SelectObject(hdc, hLight);
    MoveToEx(hdc, 0, 1, NULL);          LineTo(hdc, rc.right + 1, 1);
    SelectObject(hdc, hShadow);
    MoveToEx(hdc, 0, TOPBAR_H, NULL);   LineTo(hdc, rc.right + 1, TOPBAR_H);
    SelectObject(hdc, hLight);
    MoveToEx(hdc, 0, TOPBAR_H - 1, NULL); LineTo(hdc, rc.right + 1, TOPBAR_H - 1);

    SetBkMode(hdc, TRANSPARENT);
    rc.left = g_nextX - 0x4F; rc.right = g_nextX - 0xF;
    rc.top  = 15;             rc.bottom = 31;
    SetTextColor(hdc, GetSysColor(COLOR_3DSHADOW));
    DrawText(hdc, TEXT("Next"), -1, &rc, DT_RIGHT);
    rc.top--; rc.right--;
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawText(hdc, TEXT("Next"), -1, &rc, DT_RIGHT);

    SetTextColor(hdc, GetSysColor(COLOR_3DSHADOW));
    rc.left = g_nextX + 0x7D; rc.right = g_nextX + 0xBD;
    rc.top  = 15;             rc.bottom = 31;
    DrawText(hdc, TEXT("Colors"), -1, &rc, DT_LEFT);
    rc.top--; rc.left--;
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawText(hdc, TEXT("Colors"), -1, &rc, DT_LEFT);

    /* the king's target score, boxed at the left edge (0x41102A) */
    DrawScoreBox(hdc, 0x18, 10, (int)g_target);

    SelectObject(hdc, hOld);
    DeleteObject(hLight);
    DeleteObject(hShadow);
}

/* FUN_00410C5D - the three "next ball" previews */
static void DrawNext(HDC hdc)
{
    int i;
    if (!g_showNext) {
        for (i = 0; i < 3; i++)
            BitBlt(hdc, g_nextX + i * CELL, g_nextY, CELL, CELL,
                   g_hMemDC, 0, SPR_CELL_Y, SRCCOPY);
    } else {
        for (i = 0; i < 3; i++)
            BitBlt(hdc, g_nextX + i * CELL, g_nextY, CELL, CELL,
                   g_hMemDC, g_next[i] * CELL, SPR_NEXT_Y, SRCCOPY);
    }
}

/* FUN_0041106B - left bar: player's red king rises from the pit */
static void DrawPlayerBar(HDC hdc, BOOL drawLabel)
{
    RECT rc;

    if (drawLabel) {
        BitBlt(hdc, g_barX, g_barY - 9, 58, 9, g_hMemDC, 1, SPR_BUBBLE_Y, SRCCOPY);
        BitBlt(hdc, g_barX + 6, g_barY - 134 + g_myBarH, SPR_BAR_W,
               BAR_H - g_myBarH, g_hMemDC, SPR_BAR_X, 100, SRCCOPY);
        rc.left = g_barX - 16; rc.right  = g_barX + 74;
        rc.top  = g_barY + 8;  rc.bottom = g_barY + 24;
        SetTextColor(hdc, RGB(255, 255, 255));
        FillRect(hdc, &rc, GetStockObject(BLACK_BRUSH));
        DrawText(hdc, g_myName, -1, &rc, DT_CENTER | DT_SINGLELINE);
    }
    BitBlt(hdc, g_barX - 1, g_barY - 234 + g_myBarH, 72, 100,
           g_hMemDC, g_myFrame * SPR_KING_W + 1, SPR_PORTR_Y, SRCCOPY);
}

/* FUN_00411196 - right bar: the "Pretender" and his crown */
static void DrawKingBar(HDC hdc, BOOL drawLabel, BOOL drawKing)
{
    RECT rc;

    if (drawLabel) {
        BitBlt(hdc, g_bar2X, g_bar2Y - 9, 58, 9, g_hMemDC, 1, SPR_BUBBLE_Y, SRCCOPY);
        BitBlt(hdc, g_bar2X + 6, g_bar2Y - 134 + g_kingBarH, SPR_BAR_W,
               BAR_H - g_kingBarH, g_hMemDC, SPR_BAR_X, 100, SRCCOPY);
        rc.left = g_bar2X - 16; rc.right  = g_bar2X + 74;
        rc.top  = g_bar2Y + 8;  rc.bottom = g_bar2Y + 24;
        SetTextColor(hdc, RGB(255, 255, 255));
        FillRect(hdc, &rc, GetStockObject(BLACK_BRUSH));
        DrawText(hdc, TEXT("Pretender"), -1, &rc, DT_CENTER | DT_SINGLELINE);
    }
    if (g_crownFrame == 0 || drawKing)
        BitBlt(hdc, g_bar2X - 10, g_bar2Y - 234 + g_kingBarH, 72, 100,
               g_hMemDC, 1, SPR_PRETND_Y, SRCCOPY);
    if (g_crownFrame != 0)
        BitBlt(hdc, g_bar2X - 10, g_bar2Y + g_kingBarH - 228, 50, SPR_CROWN_H,
               g_hMemDC, g_crownFrame * SPR_CROWN_W + SPR_CROWN_X, SPR_PRETND_Y,
               SRCCOPY);
}

/* ------------------------------------------------------------------ */
/*  Game core                                                          */
/* ------------------------------------------------------------------ */

/* FUN_00410A3A */
static int CountFree(void)
{
    int x, y, n = 0;
    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++)
            if (g_board[y][x] == BALL_EMPTY)
                n++;
    return n;
}

/* FUN_004104A5 */
static void InitNext(void)
{
    int i;
    for (i = 0; i < 3; i++)
        g_next[i] = (BYTE)(Rand(BALL_COLORS) + 1);
}

/* FUN_004104C4 - BFS over empty cells.  Distances are stored in the
 * board itself biased by 8 and wiped afterwards.  On success the path
 * is left in g_pathX/g_pathY with path[1] = start ... path[len] =
 * target, len = number of steps.  (Original args: AL=fx DL=fy BL=tx
 * CL=ty.) */
static BOOL FindPath(int fx, int fy, int tx, int ty)
{
    BYTE keep = g_board[fy][fx];
    int  x, y, d, i, cx, cy, dd;
    BOOL found = FALSE;

    g_board[fy][fx] = SEED_BASE;

    for (d = SEED_BASE;; d++) {
        for (y = 0; y < BOARD_H; y++)
            for (x = 0; x < BOARD_W; x++)
                if (g_board[y][x] == d) {
                    if (y > 0         && g_board[y - 1][x] == 0) g_board[y - 1][x] = (BYTE)(d + 1);
                    if (x > 0         && g_board[y][x - 1] == 0) g_board[y][x - 1] = (BYTE)(d + 1);
                    if (y < BOARD_H-1 && g_board[y + 1][x] == 0) g_board[y + 1][x] = (BYTE)(d + 1);
                    if (x < BOARD_W-1 && g_board[y][x + 1] == 0) g_board[y][x + 1] = (BYTE)(d + 1);
                }
        if (g_board[ty][tx] == d + 1 || d + 1 >= 0x5A) {
            d++;
            break;
        }
    }
    /* d = distance value stored in the target (or 0x5A when cut off) */

    /* reconstruct the path while the distance marks are still in place */
    if (d < 0x5A && g_board[ty][tx] == d) {
        found = TRUE;
        g_pathLen = d - 7;                      /* steps = dist - 7 */
        cx = tx; cy = ty; dd = d;
        for (i = g_pathLen; i >= 1; i--) {
            g_pathY[i] = (BYTE)cy;
            g_pathX[i] = (BYTE)cx;
            if      (cy > 0 && g_board[cy - 1][cx] == dd - 1) cy--;
            else if (cx > 0 && g_board[cy][cx - 1] == dd - 1) cx--;
            else if (cy < 8 && g_board[cy + 1][cx] == dd - 1) cy++;
            else if (cx < 8 && g_board[cy][cx + 1] == dd - 1) cx++;
            dd--;
        }
    }

    for (y = 0; y < BOARD_H; y++)               /* wipe flood data */
        for (x = 0; x < BOARD_W; x++)
            if (g_board[y][x] > 7)
                g_board[y][x] = 0;
    g_board[fy][fx] = keep;

    return found;
}

/* FUN_004107A1 - screen point -> cell, FALSE when outside the grid */
static BOOL MouseToCell(int px, int py, int *x, int *y)
{
    *x = (px - g_boardX) / CELL;
    if (*x < 0 || *x >= BOARD_W) return FALSE;
    *y = (py - g_boardY) / CELL;
    if (*y < 0 || *y >= BOARD_H) return FALSE;
    return TRUE;
}

/* FUN_004107EC - detect 5+ in a row through (x,y).  Marks the balls
 * with MARK_FLAG, returns how many were marked (0 = no line).
 * Directions from the table at 0x420000. */
static int CheckLines(int x, int y, int color)
{
    static const int dirs[4][2] = { {-1, 0}, {-1, -1}, {0, -1}, {1, -1} };
    int total = 0, d;

    for (d = 0; d < 4; d++) {
        int count = 1, i, cx, cy;

        cx = x; cy = y;
        for (i = 1; i < BOARD_W; i++) {
            cx += dirs[d][0]; cy += dirs[d][1];
            if (cx < 0 || cx > 8 || cy < 0 || cy > 8) break;
            if (g_board[cy][cx] != color) break;
            count++;
            g_board[cy][cx] |= MARK_FLAG;
        }
        cx = x; cy = y;
        for (i = 1; i < BOARD_W; i++) {
            cx -= dirs[d][0]; cy -= dirs[d][1];
            if (cx < 0 || cx > 8 || cy < 0 || cy > 8) break;
            if (g_board[cy][cx] != color) break;
            count++;
            g_board[cy][cx] |= MARK_FLAG;
        }
        if (count < 5) {
            cx = x; cy = y;
            for (i = 1; i < BOARD_W; i++) {
                cx += dirs[d][0]; cy += dirs[d][1];
                if (cx < 0 || cx > 8 || cy < 0 || cy > 8) break;
                g_board[cy][cx] &= ~MARK_FLAG;
            }
            cx = x; cy = y;
            for (i = 1; i < BOARD_W; i++) {
                cx -= dirs[d][0]; cy -= dirs[d][1];
                if (cx < 0 || cx > 8 || cy < 0 || cy > 8) break;
                g_board[cy][cx] &= ~MARK_FLAG;
            }
        } else {
            total += count;
        }
    }
    if (total != 0)
        g_board[y][x] = (BYTE)(color | MARK_FLAG);
    return total;
}

/* FUN_00410976 - blink marked balls (explosion frames 8,9,0), remove
 * them, and add the count to the last colour's statistics (this quirk
 * is faithful to the original). */
static void RemoveMarked(HDC hdc)
{
    int pass, x, y, frame = 8;
    int removed = 0, lastColor = 0;

    for (pass = 0; pass < 3; pass++) {
        for (y = 0; y < BOARD_H; y++)
            for (x = 0; x < BOARD_W; x++)
                if (g_board[y][x] & MARK_FLAG) {
                    if (frame == 0) {
                        lastColor = g_board[y][x] & 0x3F;
                        g_board[y][x] = BALL_EMPTY;
                        removed++;
                    }
                    DrawCell(hdc, x, y, frame);
                }
        Sleep(TIMER_MS);
        if (pass == 0)      frame = 9;
        else if (pass == 1) frame = 0;
    }
    g_colorStat[lastColor] += removed;
    if (g_hStatWnd)
        InvalidateRect(g_hStatWnd, NULL, FALSE);
}

/* FUN_00410BC6 - 5 balls = 10 points, extras add 2, 6, 12, 20 ... */
static int CalcScore(int balls)
{
    int pts = 10, add = 2;
    while (balls-- > 5) {
        pts += add;
        add += 4;
    }
    return pts;
}

/* FUN_00411313 - add points, animate both progress bars */
static void AddScore(HDC hdc, int pts)
{
    int h;

    g_score += pts;
    DrawScoreBox(hdc, g_scoreX, g_scoreY, g_score);

    if (!g_kingDown) {
        h = BAR_H - (int)(g_score * BAR_H / g_target);
        if (h < 0) h = 0;
        while (h != g_kingBarH) {
            g_kingBarH--;
            DrawKingBar(hdc, FALSE, TRUE);
            Sleep(25);
        }
        if (g_score > (int)g_target) {
            int i;
            for (i = 0; i < 6; i++) {         /* king celebrates */
                g_myFrame++;
                g_crownFrame++;
                DrawPlayerBar(hdc, FALSE);
                DrawKingBar(hdc, FALSE, FALSE);
                Sleep(150);
            }
            for (i = 0; i < 3; i++) {         /* crown bounces   */
                g_crownFrame--;
                DrawKingBar(hdc, FALSE, FALSE);
                Sleep(150);
                g_crownFrame++;
                DrawKingBar(hdc, FALSE, FALSE);
                Sleep(150);
            }
            g_kingDown = TRUE;
        }
    } else {
        h = (int)((g_score - g_target) * BAR_H / g_target);
        if (h > BAR_H) h = BAR_H;
        while (h != g_myBarH) {
            g_myBarH++;
            DrawPlayerBar(hdc, FALSE);
            Sleep(25);
        }
    }
}

/* FUN_00410A71 - drop the next queued ball on a random free cell with
 * a birth animation (frames 6,7,0); resolve lines it completes,
 * repeating while new lines keep forming (chain reaction). */
static void AddBall(HDC hdc)
{
    int  free, n, x, y;
    BOOL again = TRUE;

    while (again) {
        again = FALSE;
        free = CountFree();
        if (free == 0) return;

        n = Rand(free);
        x = y = 9;
        for (y = 0; y < BOARD_H; y++) {
            for (x = 0; x < BOARD_W; x++) {
                if (g_board[y][x] == BALL_EMPTY) {
                    if (n == 0) goto found;
                    n--;
                }
            }
        }
found:
        if (x > 8 || y > 8) {
            MessageBox(NULL, TEXT("Report me!"), TEXT("Error"), MB_OK);
            return;
        }
        g_board[y][x] = g_next[0];
        g_next[0] = g_next[1];
        g_next[1] = g_next[2];
        g_next[2] = (BYTE)(Rand(BALL_COLORS) + 1);

        DrawCell(hdc, x, y, 6); Sleep(TIMER_MS);
        DrawCell(hdc, x, y, 7); Sleep(TIMER_MS);
        DrawCell(hdc, x, y, 0);

        if (CheckLines(x, y, g_board[y][x])) {
            RemoveMarked(hdc);
            again = TRUE;
        }
    }
}

/* FUN_00411481 */
static void NewGame(HDC hdc)
{
    int x, y, i;

    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++)
            g_board[y][x] = BALL_EMPTY;
    for (i = 0; i < BALL_MAX; i++)
        g_colorStat[i] = 0;

    InitNext();
    DrawBoard(hdc);
    for (i = 0; i < 5; i++)
        AddBall(hdc);

    g_score    = 0;
    g_kingDown = FALSE;
    DrawScoreBox(hdc, 0x18, 10, (int)g_target);          /* king's box */
    DrawScoreBox(hdc, g_scoreX, g_scoreY, g_score);      /* player box */
    DrawNext(hdc);
}

/* FUN_00411525 - end-of-round sequence: kings step down, bars drain
 * and refill, then a fresh board is dealt. */
static void ResetKings(HDC hdc)
{
    int i;

    if (g_kingDown) {
        lstrcpy(g_myName, TEXT("Pretender"));  /* 0x41153A: name reset */
        SaveHiscores();
        g_target = g_score;
        DrawPlayerBar(hdc, TRUE);
        for (i = 0; i < 6; i++) {
            g_myFrame--;
            DrawPlayerBar(hdc, FALSE);
            Sleep(150);
        }
        while (g_myBarH != 0) {
            g_myBarH -= 4;
            if (g_myBarH < 0) g_myBarH = 0;
            DrawPlayerBar(hdc, FALSE);
            Sleep(15);
        }
    }
    if (g_crownFrame != 0) {
        for (i = 0; i < 6; i++) {
            g_crownFrame--;
            DrawKingBar(hdc, FALSE, FALSE);
            Sleep(150);
        }
    }
    while (g_kingBarH != BAR_H) {
        g_kingBarH += 4;
        if (g_kingBarH > BAR_H) g_kingBarH = BAR_H;
        DrawKingBar(hdc, FALSE, TRUE);
        Sleep(15);
    }
    NewGame(hdc);
    if (g_hStatWnd)
        InvalidateRect(g_hStatWnd, NULL, FALSE);
}

/* ------------------------------------------------------------------ */
/*  High scores ("winlines.res", legacy "lines.res"; 10 records of     */
/*  1 length byte + 13 name bytes + 2 score bytes = 160 bytes)         */
/* ------------------------------------------------------------------ */

/* FUN_00411640 */
static void LoadHiscores(void)
{
    HFILE hf = _lopen("winlines.res", OF_READ);
    char  len;
    int   i;

    if (hf == HFILE_ERROR) hf = _lopen("lines.res", OF_READ);
    if (hf == HFILE_ERROR) {
        lstrcpyA(g_hiscName[0], "Handicap");    /* the "king" to beat */
        g_hiscScore[0] = 100;
        for (i = 1; i < HISCORES; i++) {
            lstrcpyA(g_hiscName[i], "- Empty -");
            g_hiscScore[i] = 0;
        }
    } else {
        for (i = 0; i < HISCORES; i++) {
            _lread(hf, &len, 1);
            _lread(hf, g_hiscName[i], NAME_LEN);
            g_hiscName[i][(unsigned char)len] = '\0';
            _lread(hf, (char *)&g_hiscScore[i], 2);
        }
        _lclose(hf);
    }
    g_target = g_hiscScore[0];
    AcpToT(g_hiscName[0], g_myName, 16);       /* player label = top name */
    g_hiscDirty = FALSE;
}

/* FUN_0041172D */
void SaveHiscores(void)
{
    HFILE hf;
    char  len;
    int   i;

    if (!g_hiscDirty) return;
    hf = _lopen("winlines.res", OF_WRITE);
    if (hf == HFILE_ERROR) hf = _lopen("lines.res", OF_WRITE);
    if (hf == HFILE_ERROR) hf = _lcreat("winlines.res", 0);
    for (i = 0; i < HISCORES; i++) {
        len = (char)lstrlenA(g_hiscName[i]);
        _lwrite(hf, &len, 1);
        _lwrite(hf, g_hiscName[i], NAME_LEN);
        _lwrite(hf, (const char *)&g_hiscScore[i], 2);
    }
    _lclose(hf);
}

/* FUN_004117C5 - insert the score into the table if it qualifies */
static void CheckHiscore(HWND hwnd)
{
    int idx = 0, i;

    while (idx < HISCORES && g_score <= g_hiscScore[idx])
        idx++;
    if (idx == HISCORES) return;

    g_hiscDirty = TRUE;
    for (i = HISCORES - 1; i != idx; i--) {
        lstrcpyA(g_hiscName[i], g_hiscName[i - 1]);
        g_hiscScore[i] = g_hiscScore[i - 1];
    }
    DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_NAME), hwnd, NameDlgProc, 0);
    TToAcp(g_myName, g_hiscName[idx], NAME_LEN + 1);
    g_hiscScore[idx] = (WORD)g_score;
    DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_SCORES), hwnd, ScoresDlgProc, 0);
}

/* ------------------------------------------------------------------ */
/*  Dialog procedures                                                  */
/* ------------------------------------------------------------------ */

/* shared tail of every modal dialog (WM_INITDIALOG) */
static INT_PTR CenterDlg(HWND hdlg)
{
    RECT rc;
    SetFocus(hdlg);
    GetWindowRect(hdlg, &rc);
    MoveWindow(hdlg,
               (g_screenW - (rc.right - rc.left)) / 2,
               (g_screenH - (rc.bottom - rc.top)) / 2,
               rc.right - rc.left, rc.bottom - rc.top, TRUE);
    return TRUE;
}

/* FUN_004121B5 - "Rules" */
static INT_PTR CALLBACK RulesDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: return CenterDlg(hdlg);
    case WM_CLOSE:                        /* 0x4122xx falls through to EndDialog */
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || msg == WM_CLOSE) { EndDialog(hdlg, 0); return TRUE; }
        break;
    }
    return FALSE;
}

/* FUN_00412279 - "High Scores"; the list box is only a frame, the ten
 * records are painted straight onto the dialog. */
static INT_PTR CALLBACK ScoresDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC    hdc  = BeginPaint(hdlg, &ps);
        HFONT  font = CreateFont(18, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0,
                                 TEXT("MS Sans Serif"));
        HGDIOBJ old = SelectObject(hdc, font);
        RECT rc;
        TCHAR buf[64], num[16], nameT[16];
        int  i;

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));
        GetClientRect(hdlg, &rc);
        rc.left += 16; rc.right -= 16; rc.top += 16;
        for (i = 0; i < HISCORES; i++) {
            rc.bottom = rc.top + 16;
            Itoa(i + 1, buf);
            lstrcat(buf, TEXT(". "));
            AcpToT(g_hiscName[i], nameT, 16);
            lstrcat(buf, nameT);
            DrawText(hdc, buf, -1, &rc, DT_LEFT);
            Itoa(g_hiscScore[i], num);
            DrawText(hdc, num, -1, &rc, DT_RIGHT);
            rc.top += 16;
        }
        SelectObject(hdc, old);
        DeleteObject(font);
        EndPaint(hdlg, &ps);
        return TRUE;
    }
    case WM_INITDIALOG: return CenterDlg(hdlg);
    case WM_CLOSE:
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || msg == WM_CLOSE) { EndDialog(hdlg, 0); return TRUE; }
        break;
    }
    return FALSE;
}

/* FUN_00412454 - name entry after a new high score */
static INT_PTR CALLBACK NameDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        CenterDlg(hdlg);
        SetDlgItemText(hdlg, IDC_NAME, TEXT("Pretender"));
        return TRUE;
    case WM_CLOSE:                              /* original quirk: X behaves
                                                   like pressing Ok */
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || msg == WM_CLOSE) {
            TCHAR name[20];
            int  len, i;

            GetDlgItemText(hdlg, IDC_NAME, name, 16);
            len = lstrlen(name);
            if (len == 0) {
                MessageBox(hdlg, TEXT("You must enter some name"), TEXT("Error"), MB_OK);
                return TRUE;
            }
            if (len >= 14) {
                MessageBox(hdlg, TEXT("Name too long"), TEXT("Error"), MB_OK);
                return TRUE;
            }
            for (i = 0; i < len; i++)
                if (name[i] & 0x80) {
                    MessageBox(hdlg, TEXT("You can use only latin characters"),
                               TEXT("Error"), MB_OK);
                    return TRUE;
                }
            lstrcpy(g_myName, name);
            EndDialog(hdlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* FUN_0041260F - "About": masked-blit of the two king portraits */
static INT_PTR CALLBACK AboutDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC     hdc  = BeginPaint(hdlg, &ps);
        HBITMAP bmp  = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_ABOUT_KING));
        HBITMAP mask = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_ABOUT_MASK));
        HDC     mem  = CreateCompatibleDC(hdc);
        HGDIOBJ old  = SelectObject(mem, mask);
        BitBlt(hdc, 10, 32, 67, 50, mem, 0, 0, SRCAND);
        SelectObject(mem, bmp);
        BitBlt(hdc, 10, 32, 67, 50, mem, 0, 0, SRCPAINT);
        SelectObject(mem, old);
        DeleteDC(mem);
        DeleteObject(mask);
        DeleteObject(bmp);
        EndPaint(hdlg, &ps);
        return TRUE;
    }
    case WM_INITDIALOG: return CenterDlg(hdlg);
    case WM_CLOSE:
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || msg == WM_CLOSE) { EndDialog(hdlg, 0); return TRUE; }
        break;
    }
    return FALSE;
}

/* FUN_0041277C - modeless "Statistics": ball census per colour */
static INT_PTR CALLBACK StatsDlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: {
        RECT rc;
        GetWindowRect(g_hMainWnd, &rc);
        MoveWindow(hdlg, g_boardX + 0x154 + rc.left, rc.top + 32, 0xA4, 0x136,
                   FALSE);
        SetFocus(g_hMainWnd);
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC    hdc  = BeginPaint(hdlg, &ps);
        int    cnt[BALL_MAX + 1] = {0};
        HFONT  font = CreateFont(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0,
                                 TEXT("MS Sans Serif"));
        HGDIOBJ old = SelectObject(hdc, font);
        RECT rc;
        TCHAR buf[64], num[16], nameT[16];
        int  c, x, y;

        for (y = 0; y < BOARD_H; y++)
            for (x = 0; x < BOARD_W; x++)
                cnt[g_board[y][x] & 0x3F]++;

        SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
        SetTextColor(hdc, RGB(0, 0, 0));
        GetClientRect(hdlg, &rc);
        rc.left += 0x2C;
        rc.bottom = rc.top + 0x23;
        for (c = 0; c < BALL_MAX; c++) {
            BitBlt(hdc, rc.left - 0x2C, rc.top, CELL, CELL,
                   g_hMemDC, c * CELL, SPR_CELL_Y, SRCCOPY);
            Itoa(cnt[c], buf);
            lstrcat(buf, TEXT(" ("));
            Itoa(cnt[c] * 100 / (BOARD_W * BOARD_H), num);
            lstrcat(buf, num);
            lstrcat(buf, TEXT("%)"));
            if (c != 0) {
                lstrcat(buf, TEXT(", del-"));
                Itoa(g_colorStat[c], num);
                lstrcat(buf, num);
            }
            lstrcat(buf, TEXT("          "));
            DrawText(hdc, buf, -1, &rc, DT_SINGLELINE);
            rc.top    += CELL;
            rc.bottom += CELL;
        }
        SelectObject(hdc, old);
        DeleteObject(font);
        EndPaint(hdlg, &ps);
        return TRUE;
    }
    case WM_CLOSE:
        DestroyWindow(hdlg);
        g_hStatWnd    = NULL;
        g_statVisible = FALSE;
        CheckMenuItem(GetMenu(g_hMainWnd), CM_STATS, MF_UNCHECKED);
        return TRUE;
    }
    return FALSE;
}

/* Extra fix (not original behaviour): centre the board horizontally in
 * the client area and vertically between the top bar and the window
 * bottom for ANY window size.  The original formula,
 * (clientH-376)/2+46, only centres properly when the window is taller
 * than its minimum and was measured from WM_SIZE alone - which modern
 * Windows may deliver after the first paint, leaving the board at (0,0). */
static void LayoutBoard(int clientW, int clientH)
{
    g_boardX = (clientW - BOARD_PIX) / 2;
    if (g_boardX < 0) g_boardX = 0;
    g_boardY = TOPBAR_H + (clientH - TOPBAR_H - BOARD_PIX) / 2 - 2;
    if (g_boardY < 0) g_boardY = 0;
}

/* ------------------------------------------------------------------ */
/*  Main window procedure - FUN_0041187E                               */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    HDC hdc;

    switch (msg) {
    case WM_GETMINMAXINFO: {                    /* min size 620x420 (0x4118xx:
                                                    MINMAXINFO+0x18 = ptMinTrackSize) */
        MINMAXINFO *mmi = (MINMAXINFO *)lp;
        mmi->ptMinTrackSize.x = WIN_W;
        mmi->ptMinTrackSize.y = WIN_H;
        return 0;
    }

    case WM_CREATE: {
        RECT rc;

        g_hInst = ((LPCREATESTRUCT)lp)->hInstance;
        hdc = GetDC(hwnd);
        LoadSprites(hdc);
        g_hMemDC  = CreateCompatibleDC(hdc);
        g_hOldBmp = SelectObject(g_hMemDC, g_hSprites);
        ReleaseDC(hwnd, hdc);
        LoadHiscores();
        SetTimer(hwnd, 0x1000, TIMER_MS, NULL);
        g_tick  = 0;
        g_state = 0;
        GetClientRect(hwnd, &rc);
        LayoutBoard(rc.right, rc.bottom);   /* extra fix: origin before first paint */
        g_scoreX    = rc.right - 0x60;
        g_scoreY    = 10;
        g_nextX     = (rc.right - 0x6C) / 2;
        g_nextY     = 5;
        g_showNext  = TRUE;
        g_barX      = ((rc.right - 0x148) / 2 - 0x3A) / 2;
        g_barY      = (rc.bottom - 0xEA) / 2 + 0xEA;
        g_myBarH    = 0;
        g_myFrame   = 0;
        g_bar2X     = (rc.right - 0x148) / 2 + 0x148 +
                      ((rc.right - 0x148) / 2 - 0x3A) / 2;
        g_bar2Y     = g_barY;
        g_kingBarH  = BAR_H;
        g_crownFrame= 0;
        Srand(GetTickCount());
        hdc = GetDC(hwnd);
        NewGame(hdc);
        ReleaseDC(hwnd, hdc);
        return 0;
    }

    case WM_DESTROY:
        SaveHiscores();
        if (g_hStatWnd) DestroyWindow(g_hStatWnd);
        SelectObject(g_hMemDC, g_hOldBmp);
        DeleteDC(g_hMemDC);
        DeleteObject(g_hSprites);
        DeleteObject(g_hPalette);
        KillTimer(hwnd, 0x1000);
        PostQuitMessage(0);
        return 0;

    case WM_SIZE: {
        RECT rc;
        int  half;
        LayoutBoard((int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        GetClientRect(hwnd, &rc);
        half      = (rc.right - 0x148) / 2;
        g_barY    = (rc.bottom - 0xEA) / 2 + 0xEA;
        g_scoreX  = rc.right - 0x60;
        g_nextX   = (rc.right - 0x6C) / 2;
        g_barX    = (half - 0x3A) / 2;
        g_bar2X   = half + 0x148 + (half - 0x3A) / 2;
        g_bar2Y   = g_barY;
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc;
        if (IsIconic(hwnd)) return 0;
        hdc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetStockObject(BLACK_BRUSH));   /* no stale pixels */
        SelectPalette(hdc, g_hPalette, FALSE);
        RealizePalette(hdc);
        DrawBoard(hdc);
        DrawTopBar(hwnd, hdc);
        DrawScoreBox(hdc, g_scoreX, g_scoreY, g_score);
        DrawNext(hdc);
        DrawPlayerBar(hdc, TRUE);
        DrawKingBar(hdc, TRUE, TRUE);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:
        g_tick++;
        if (IsIconic(hwnd)) return 0;
        if (g_state != 1) return 0;
        hdc = GetDC(hwnd);
        DrawCell(hdc, g_selX, g_selY, g_tick % 6);   /* fast roll/blink */
        ReleaseDC(hwnd, hdc);
        return 0;

    case WM_LBUTTONDOWN: {
        int x, y;
        if (!MouseToCell((short)LOWORD(lp), (short)HIWORD(lp), &x, &y)) {
            g_downOutside = TRUE;
            return 0;
        }
        g_downOutside = FALSE;
        g_curX = x;
        g_curY = y;
        return 0;
    }

    case WM_LBUTTONUP: {
        int  x, y, n, i, j, frame;

        if (!MouseToCell((short)LOWORD(lp), (short)HIWORD(lp), &x, &y))
            return 0;
        if (g_downOutside) return 0;
        if (g_curX != x || g_curY != y) return 0;

        if (g_state == 0) {
            if (g_board[y][x] == BALL_EMPTY) return 0;
            g_state = 1;
            g_selX  = (BYTE)x;
            g_selY  = (BYTE)y;
            g_tick  = 3;
            return 0;
        }
        if (g_state != 1) return 0;

        if (g_board[y][x] != BALL_EMPTY) {          /* re-select */
            hdc = GetDC(hwnd);
            DrawCell(hdc, g_selX, g_selY, 0);
            ReleaseDC(hwnd, hdc);
            g_selX = (BYTE)x;
            g_selY = (BYTE)y;
            g_tick |= 3;
            return 0;
        }

        /* move the ball along the BFS path */
        g_moveColor = g_board[g_selY][g_selX];
        if (!FindPath(g_selX, g_selY, x, y)) {
            MessageBeep(MB_ICONASTERISK);
            /* pathfinding wipes any flood marks (values > 7) silently;
             * repaint so the board reflects the new state at once */
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        g_state = 0;
        hdc = GetDC(hwnd);
        for (j = 0; j < g_pathLen - 1; j++) {
            frame = j % 6;
            g_board[g_pathY[j + 1]][g_pathX[j + 1]] = BALL_EMPTY;
            DrawCell(hdc, g_pathX[j + 1], g_pathY[j + 1], frame);
            g_board[g_pathY[j + 2]][g_pathX[j + 2]] = g_moveColor;
            DrawCell(hdc, g_pathX[j + 2], g_pathY[j + 2], frame);
            Sleep(TIMER_MS);
        }
        /* settle the rolling animation on a frame boundary */
        for (i = j % 6; i != 0; i = (i + 1) % 6)
            DrawCell(hdc, x, y, i);

        n = CheckLines(x, y, g_moveColor);
        if (n != 0) {
            RemoveMarked(hdc);
            AddScore(hdc, CalcScore(n));
            ReleaseDC(hwnd, hdc);
            return 0;
        }
        for (i = 0; i < 3; i++)
            AddBall(hdc);
        if (CountFree() != 0) {
            DrawNext(hdc);
            ReleaseDC(hwnd, hdc);
            if (g_hStatWnd)
                InvalidateRect(g_hStatWnd, NULL, FALSE);
            return 0;
        }
        ReleaseDC(hwnd, hdc);
        MessageBox(hwnd, TEXT("Game over!"), APP_TITLE, MB_ICONWARNING);
        CheckHiscore(hwnd);
        hdc = GetDC(hwnd);
        ResetKings(hdc);
        ReleaseDC(hwnd, hdc);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case CM_NEW:
            g_state = 0;
            CheckHiscore(hwnd);
            hdc = GetDC(hwnd);
            ResetKings(hdc);
            ReleaseDC(hwnd, hdc);
            return 0;
        case CM_SCORES:
            DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_SCORES), hwnd,
                            ScoresDlgProc, 0);
            return 0;
        case CM_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case CM_NEXT:
            g_showNext = !g_showNext;
            CheckMenuItem(GetMenu(hwnd), CM_NEXT,
                          g_showNext ? MF_CHECKED : MF_UNCHECKED);
            hdc = GetDC(hwnd);
            DrawNext(hdc);
            ReleaseDC(hwnd, hdc);
            return 0;
        case CM_STATS:
            g_statVisible = !g_statVisible;
            if (!g_statVisible) {
                if (g_hStatWnd) DestroyWindow(g_hStatWnd);
                g_hStatWnd = NULL;
            } else {
                g_hStatWnd = CreateDialogParam(g_hInst,
                                MAKEINTRESOURCE(IDD_STATS), hwnd,
                                StatsDlgProc, 0);
            }
            CheckMenuItem(GetMenu(hwnd), CM_STATS,
                          g_statVisible ? MF_CHECKED : MF_UNCHECKED);
            return 0;
        case CM_RULES:
            DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_RULES), hwnd,
                            RulesDlgProc, 0);
            return 0;
        case CM_ABOUT:
            DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hwnd,
                            AboutDlgProc, 0);
            return 0;
        }
        break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------------ */
/*  Entry - FUN_00410000 + FUN_00410178                                */
/* ------------------------------------------------------------------ */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int nCmdShow)
{
    WNDCLASS  wc;
    HWND      hwnd;
    MSG       msg;

    g_hInst = hInst;
    wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;    /* 0x23 */
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU1);
    wc.lpszClassName = APP_CLASS;
    if (!RegisterClass(&wc)) return 0;

    g_screenW = GetSystemMetrics(SM_CXSCREEN);
    g_screenH = GetSystemMetrics(SM_CYSCREEN);

    hwnd = CreateWindowEx(0, APP_CLASS, APP_TITLE,
                           WS_OVERLAPPEDWINDOW,          /* 0xCF0000 */
                           (g_screenW - WIN_W) / 2, (g_screenH - WIN_H) / 2,
                           WIN_W, WIN_H,
                           NULL, NULL, hInst, NULL);
    g_hMainWnd = hwnd;
    g_hAccel   = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCEL1));
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        if (g_hStatWnd && IsDialogMessage(g_hStatWnd, &msg))
            continue;
        if (!TranslateAccelerator(hwnd, g_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}
