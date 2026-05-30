/*
 * use_case_03_1.c - UC3.1 Validation: GUI message queue + Win32 window
 *
 * Tests:
 *   1. gui_msg_queue_t full lifecycle (nominal + robustness)
 *   2. gui_init / gui_shutdown lifecycle (nominal + idempotency)
 *   3. gui_* NULL-parameter robustness (headless)
 *   4. gui_open_window / gui_close_window (manual - require display)
 *
 * TEST MATRIX - UC3.1:
 *   [N] Nominal    : 14 tests - msg_queue lifecycle, gui init/shutdown
 *   [R] Robustness : 11 tests - NULL params, capacity 0, queue full/empty
 *   [S] Skipped    :  3 tests - window open/visible/close (make manual)
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

#include "use_cases.h"
#include <string.h>

int g_uc_fails = 0;

int main(void)
{
    gui_msg_queue_t  hQ;
    gui_event_t      tEvt;
    gui_event_t      tGot;
    gui_window_t     hWnd;
    gui_wnd_desc_t   tDesc;
    st_error_t       eRet;
    int              iW;
    int              iH;
    int              i;

    /* ---------------------------------------------------------------- */
    printf("\n");
    printf("================================================================\n");
    printf(" UC3.1 - GUI message queue + Win32 window infrastructure\n");
    printf("================================================================\n");

    /* Minimal trace init (log to file, console closed) */
    trace_init(ST_FALSE);

    /* ================================================================ */
    printf("\n--- Test group 1: gui_msg_queue_t nominal ---\n");
    /* ================================================================ */

    hQ = NULL;

    /* INTENT: create a queue of capacity 4 */
    eRet = gui_msg_create(&hQ, 4);
    UC_TEST("[N] gui_msg_create(4) -> ST_NO_ERROR", eRet == ST_NO_ERROR);
    UC_TEST("[N] gui_msg_create: handle not NULL", hQ != NULL);

    /* INTENT: post one event, get it back intact */
    memset(&tEvt, 0, sizeof(tEvt));
    tEvt.eType = GUI_EVT_PAINT;
    eRet = gui_msg_post(hQ, &tEvt);
    UC_TEST("[N] gui_msg_post one event -> ST_NO_ERROR",
            eRet == ST_NO_ERROR);

    memset(&tGot, 0xFF, sizeof(tGot));
    eRet = gui_msg_get(hQ, &tGot, ST_FALSE);
    UC_TEST("[N] gui_msg_get (non-blocking, has event) -> ST_NO_ERROR",
            eRet == ST_NO_ERROR);
    UC_TEST("[N] gui_msg_get: event type preserved",
            tGot.eType == GUI_EVT_PAINT);

    /* INTENT: post two different events, get both in FIFO order */
    memset(&tEvt, 0, sizeof(tEvt));
    tEvt.eType = GUI_EVT_RESIZE;
    tEvt.uData.tResize.iWidth  = 800;
    tEvt.uData.tResize.iHeight = 600;
    gui_msg_post(hQ, &tEvt);

    memset(&tEvt, 0, sizeof(tEvt));
    tEvt.eType = GUI_EVT_CLOSE;
    gui_msg_post(hQ, &tEvt);

    memset(&tGot, 0, sizeof(tGot));
    gui_msg_get(hQ, &tGot, ST_FALSE);
    UC_TEST("[N] FIFO order: first dequeued is RESIZE",
            tGot.eType == GUI_EVT_RESIZE);
    UC_TEST("[N] FIFO: RESIZE carries correct width",
            tGot.uData.tResize.iWidth == 800);

    memset(&tGot, 0, sizeof(tGot));
    gui_msg_get(hQ, &tGot, ST_FALSE);
    UC_TEST("[N] FIFO order: second dequeued is CLOSE",
            tGot.eType == GUI_EVT_CLOSE);

    /* INTENT: fill queue to capacity then drain */
    for (i = 0; i < 4; i++)
    {
        memset(&tEvt, 0, sizeof(tEvt));
        tEvt.eType           = GUI_EVT_SCROLL;
        tEvt.uData.tScroll.iDelta = i + 1;
        eRet = gui_msg_post(hQ, &tEvt);
        UC_TEST("[N] fill to capacity: each post succeeds",
                eRet == ST_NO_ERROR);
    }

    for (i = 0; i < 4; i++)
    {
        memset(&tGot, 0, sizeof(tGot));
        eRet = gui_msg_get(hQ, &tGot, ST_FALSE);
        UC_TEST("[N] drain: each get succeeds", eRet == ST_NO_ERROR);
    }

    /* INTENT: destroy releases resources, sets handle to NULL */
    eRet = gui_msg_destroy(&hQ);
    UC_TEST("[N] gui_msg_destroy -> ST_NO_ERROR", eRet == ST_NO_ERROR);
    UC_TEST("[N] gui_msg_destroy: handle set to NULL", hQ == NULL);

    /* ================================================================ */
    printf("\n--- Test group 2: gui_msg_queue_t robustness ---\n");
    /* ================================================================ */

    /* INTENT: NULL out-pointer rejected */
    eRet = gui_msg_create(NULL, 4);
    UC_TEST("[R] gui_msg_create(NULL out) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT: capacity=0 rejected */
    hQ = NULL;
    eRet = gui_msg_create(&hQ, 0);
    UC_TEST("[R] gui_msg_create(capacity=0) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[R] gui_msg_create(0): handle stays NULL", hQ == NULL);

    /* Create a valid queue for remaining robustness tests */
    gui_msg_create(&hQ, 2);

    /* INTENT: post to NULL queue rejected */
    memset(&tEvt, 0, sizeof(tEvt));
    tEvt.eType = GUI_EVT_KEY_DOWN;
    eRet = gui_msg_post(NULL, &tEvt);
    UC_TEST("[R] gui_msg_post(NULL queue) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT: post with NULL event rejected */
    eRet = gui_msg_post(hQ, NULL);
    UC_TEST("[R] gui_msg_post(NULL event) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT: get from NULL queue rejected */
    eRet = gui_msg_get(NULL, &tGot, ST_FALSE);
    UC_TEST("[R] gui_msg_get(NULL queue) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT: get into NULL output rejected */
    eRet = gui_msg_get(hQ, NULL, ST_FALSE);
    UC_TEST("[R] gui_msg_get(NULL event out) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT: get on empty queue (non-blocking) returns ST_ERROR */
    eRet = gui_msg_get(hQ, &tGot, ST_FALSE);
    UC_TEST("[R] gui_msg_get on empty queue -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT: post beyond capacity returns ST_ERROR */
    memset(&tEvt, 0, sizeof(tEvt));
    tEvt.eType = GUI_EVT_PAINT;
    gui_msg_post(hQ, &tEvt);
    gui_msg_post(hQ, &tEvt);
    eRet = gui_msg_post(hQ, &tEvt);  /* 3rd post on capacity-2 queue */
    UC_TEST("[R] gui_msg_post beyond capacity -> ST_ERROR",
            eRet == ST_ERROR);

    gui_msg_destroy(&hQ);

    /* INTENT: destroy NULL pphQueue rejected */
    eRet = gui_msg_destroy(NULL);
    UC_TEST("[R] gui_msg_destroy(NULL) -> ST_ERROR", eRet == ST_ERROR);

    /* INTENT: destroy already-NULL handle rejected */
    hQ = NULL;
    eRet = gui_msg_destroy(&hQ);
    UC_TEST("[R] gui_msg_destroy(*pphQueue=NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    /* ================================================================ */
    printf("\n--- Test group 3: gui_init / gui_shutdown lifecycle ---\n");
    /* ================================================================ */

    /* INTENT: gui_init initialises Win32 WNDCLASS */
    eRet = gui_init();
    UC_TEST("[N] gui_init() -> ST_NO_ERROR", eRet == ST_NO_ERROR);

    /* INTENT: second call is idempotent */
    eRet = gui_init();
    UC_TEST("[N] gui_init() idempotent -> ST_NO_ERROR",
            eRet == ST_NO_ERROR);

    /* INTENT: gui_shutdown cleans up without crash */
    eRet = gui_shutdown();
    UC_TEST("[N] gui_shutdown() -> ST_NO_ERROR", eRet == ST_NO_ERROR);

    /* INTENT: second shutdown is a safe no-op */
    eRet = gui_shutdown();
    UC_TEST("[N] gui_shutdown() idempotent -> ST_NO_ERROR",
            eRet == ST_NO_ERROR);

    /* ================================================================ */
    printf("\n--- Test group 4: gui_* NULL-parameter robustness ---\n");
    /* ================================================================ */

    /* Re-init for the NULL-param tests */
    gui_init();

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle = "UC3.1 Test";
    tDesc.eType   = GUI_WND_DIR;

    /* INTENT: NULL ptDesc rejected */
    hWnd = NULL;
    eRet = gui_open_window(NULL, &hWnd);
    UC_TEST("[R] gui_open_window(NULL desc) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[R] gui_open_window(NULL desc): hWnd stays NULL",
            hWnd == NULL);

    /* INTENT: NULL phWnd rejected */
    eRet = gui_open_window(&tDesc, NULL);
    UC_TEST("[R] gui_open_window(NULL phWnd) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT: close NULL handle rejected */
    eRet = gui_close_window(NULL);
    UC_TEST("[R] gui_close_window(NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT: invalidate NULL rejected */
    eRet = gui_invalidate(NULL);
    UC_TEST("[R] gui_invalidate(NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT: get_size with NULL hWnd rejected */
    iW = 0; iH = 0;
    eRet = gui_get_size(NULL, &iW, &iH);
    UC_TEST("[R] gui_get_size(NULL hWnd) -> ST_ERROR",
            eRet == ST_ERROR);

    gui_shutdown();

    /* ================================================================ */
    printf("\n--- Test group 5: window open / close (manual) ---\n");
    /* ================================================================ */

    TEST_SKIP("[S] gui_open_window() - window visible (run make manual)");
    TEST_SKIP("[S] window: dark background, title bar visible");
    TEST_SKIP("[S] gui_close_window() - window closes cleanly");

    /* ================================================================ */
    printf("\n[cleanup] trace_shutdown()\n");
    trace_shutdown();

    printf("\n================================================================\n");
    if (g_uc_fails == 0)
    {
        printf(" UC3.1 PASSED - 0 failures\n");
    }
    else
    {
        printf(" UC3.1 FAILED - %d failure(s)\n", g_uc_fails);
    }
    printf("================================================================\n\n");

    return (g_uc_fails > 0) ? 1 : 0;
}
