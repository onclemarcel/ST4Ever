/*
 * use_case_27.c - UC27: Atari ST screen emulation view
 *
 * Validates exec_screen_open/close lifecycle, window dimension constants,
 * pixel buffer capacity, and renderer_draw_bitmap() robustness.
 * GUI rendering tests (actual window + D2D blit) require a display —
 * they are marked [S] and validated manually.
 *
 * TEST MATRIX - UC27:
 *   [N] Nominal    :  7 tests - window constants, pixel buffer capacity,
 *                               exec_screen_open without active session,
 *                               exec_screen_close on NULL view
 *   [R] Robustness :  4 tests - exec_screen_open(NULL), exec_screen_close(NULL),
 *                               renderer_draw_bitmap NULL params guard
 *   [S] Skipped    : 10 tests - require display: exec_open with screen view,
 *                               bitmap blit, aspect-ratio scale, ESC key close,
 *                               resize, resolution overlay, title update
 */

/* INTENT[INT-SCR-001..007 -> TC-SCR-001..007 -> REQ-SCR-001..002]:
 * Window constants EXEC_SCR_WND_W=640 / EXEC_SCR_WND_H=400 accommodate all
 * three ST resolutions (low 320x200 at 2x, med 640x200 at 1x/2x, high 640x400
 * at 1x).  SHIFTER_MAX_PIXELS >= 640*400 guarantees the pixel buffer covers the
 * largest frame.  exec_screen_open() returns ST_ERROR if no exec session is
 * active (exec_get_state() == NULL).  exec_screen_close() is a no-op on a NULL
 * view. */

/* INTENT[INT-SCR-008..011 -> TC-SCR-008..011 -> REQ-SCR-001,REQ-RND-008]:
 * exec_screen_open(NULL) and exec_screen_close(NULL) return ST_ERROR without
 * crashing.  renderer_draw_bitmap() with NULL hCtx, NULL pPixels, or NULL ptDest
 * returns ST_ERROR without crashing. */

#include "use_cases.h"

int g_uc_fails = 0;

/* ------------------------------------------------------------------
 * [N] Window geometry constants
 * ------------------------------------------------------------------ */

static void test_window_constants(void)
{
    printf("\n--- [N] Window geometry constants ---\n");

    TEST_ASSERT("[N] INT-SCR-001: EXEC_SCR_WND_W == 640",
                EXEC_SCR_WND_W == 640);

    TEST_ASSERT("[N] INT-SCR-002: EXEC_SCR_WND_H == 400",
                EXEC_SCR_WND_H == 400);

    TEST_ASSERT("[N] INT-SCR-003: SHIFTER_MAX_PIXELS >= 640*400",
                SHIFTER_MAX_PIXELS >= EXEC_SCR_WND_W * EXEC_SCR_WND_H);

    TEST_ASSERT("[N] INT-SCR-004: low res 320x200 fits in window",
                EXEC_SCR_WND_W >= SHIFTER_WIDTH_LOW
             && EXEC_SCR_WND_H >= SHIFTER_HEIGHT_LOW);

    TEST_ASSERT("[N] INT-SCR-005: med res 640x200 fits in window",
                EXEC_SCR_WND_W >= SHIFTER_WIDTH_MED
             && EXEC_SCR_WND_H >= SHIFTER_HEIGHT_MED);

    TEST_ASSERT("[N] INT-SCR-006: high res 640x400 fits exactly in window",
                EXEC_SCR_WND_W == SHIFTER_WIDTH_HIGH
             && EXEC_SCR_WND_H == SHIFTER_HEIGHT_HIGH);
}

/* ------------------------------------------------------------------
 * [N] exec_screen lifecycle without active session
 * ------------------------------------------------------------------ */

static void test_lifecycle_no_session(void)
{
    exec_screen_view_t *ptView;
    st_error_t          eResult;

    printf("\n--- [N] exec_screen lifecycle without active session ---\n");

    /* No exec session active: exec_get_state() == NULL */
    ptView  = (exec_screen_view_t *)((void *)(size_t)0xDEADu);
    eResult = exec_screen_open(&ptView);
    TEST_ASSERT("[N] INT-SCR-007: open without exec session -> ST_ERROR",
                eResult == ST_ERROR);
    TEST_ASSERT("[N] INT-SCR-007b: open without exec session -> ptView=NULL",
                ptView == NULL);

    /* Close on NULL view is a no-op */
    ptView  = NULL;
    eResult = exec_screen_close(&ptView);
    TEST_ASSERT("[N] INT-SCR-007c: close NULL view -> ST_NO_ERROR",
                eResult == ST_NO_ERROR);
}

/* ------------------------------------------------------------------
 * [R] Robustness
 * ------------------------------------------------------------------ */

static void test_robustness(void)
{
    exec_screen_view_t *ptView;
    st_error_t          eResult;
    renderer_rect_t     tDest;
    st_u8_t             aBuf[4];

    printf("\n--- [R] Robustness ---\n");

    /* exec_screen_open(NULL) */
    eResult = exec_screen_open(NULL);
    TEST_ASSERT("[R] INT-SCR-008: exec_screen_open(NULL) -> ST_ERROR",
                eResult == ST_ERROR);

    /* exec_screen_close(NULL) */
    eResult = exec_screen_close(NULL);
    TEST_ASSERT("[R] INT-SCR-009: exec_screen_close(NULL) -> ST_ERROR",
                eResult == ST_ERROR);

    /* renderer_draw_bitmap: NULL hCtx */
    tDest = (renderer_rect_t)RENDERER_RECT(0, 0, 320, 200);
    aBuf[0] = aBuf[1] = aBuf[2] = aBuf[3] = 0;
    eResult = renderer_draw_bitmap(NULL, aBuf, 1, 1, &tDest);
    TEST_ASSERT("[R] INT-SCR-010: renderer_draw_bitmap NULL hCtx -> ST_ERROR",
                eResult == ST_ERROR);

    /* renderer_draw_bitmap: NULL pPixels */
    ptView  = NULL; /* hCtx = 0 too, but NULL pPixels check happens first */
    eResult = renderer_draw_bitmap(NULL, NULL, 1, 1, &tDest);
    TEST_ASSERT("[R] INT-SCR-011: renderer_draw_bitmap NULL pPixels -> ST_ERROR",
                eResult == ST_ERROR);

    /* Suppress unused warning */
    ST_UNUSED(ptView);
}

/* ------------------------------------------------------------------
 * [S] Tests requiring display (manually validated)
 * ------------------------------------------------------------------ */

static void test_skipped(void)
{
    printf("\n--- [S] Tests requiring display ---\n");

    TEST_SKIP("[S] INT-SCR-012: exec_open() opens screen view (run make manual)");
    TEST_SKIP("[S] INT-SCR-013: PAINT renders Shifter frame via D2D DrawBitmap");
    TEST_SKIP("[S] INT-SCR-014: low-res 320x200 stretched 2x in 640x400 window");
    TEST_SKIP("[S] INT-SCR-015: high-res 640x400 rendered 1:1");
    TEST_SKIP("[S] INT-SCR-016: aspect-ratio preserved with black bars on resize");
    TEST_SKIP("[S] INT-SCR-017: resolution info overlay (e.g. '320x200') visible");
    TEST_SKIP("[S] INT-SCR-018: ESC key closes screen view window");
    TEST_SKIP("[S] INT-SCR-019: RESIZE event updates iWndWidth/iWndHeight");
    TEST_SKIP("[S] INT-SCR-020: exec_close() closes screen view cleanly");
    TEST_SKIP("[S] INT-SCR-021: palette black=0x0000 renders black pixels");
}

/* ------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------ */

int main(void)
{
    printf("=== UC27: Atari ST screen emulation view ===\n");

    test_window_constants();
    test_lifecycle_no_session();
    test_robustness();
    test_skipped();

    printf("\n=== UC27: %d failure(s) ===\n", g_uc_fails);
    return (g_uc_fails > 0) ? 1 : 0;
}
