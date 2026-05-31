/*
 * use_case_03_1.c - UC3.1 Validation: GUI message queue + Win32 window
 *
 * SRTD reference: SRTD.md §5.10–§5.11 — test cases TC-GUI-001..018
 * Traceability chain per INTENT block:
 *   INTENT[INT-GUI-NNN → TC-GUI-NNN → REQ-GUI-NNN]: intent text
 *
 * TEST MATRIX - UC3.1:
 *   [N] Nominal    : 22 tests - gui_msg_queue_t full lifecycle (incl.
 *                               loop assertions), gui_init/shutdown
 *   [R] Robustness : 17 tests - NULL params, capacity 0, queue full,
 *                               queue empty, NULL window handles
 *   [S] Skipped    :  3 tests - window open/visible/close (make manual)
 *
 * Test groups:
 *   Group 1: gui_msg_queue_t nominal  (create, post, get, FIFO,
 *             fill/drain to capacity, destroy)
 *   Group 2: gui_msg_queue_t robustness (NULL params, capacity 0,
 *             queue full, queue empty)
 *   Group 3: gui lifecycle nominal    (gui_init, gui_shutdown,
 *             idempotency of both)
 *   Group 4: gui_* NULL-param robustness (open_window, close_window,
 *             invalidate, get_size)
 *   Group 5: window open / close      (visual — make manual)
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

    printf("\n");
    printf("================================================================\n");
    printf(" UC3.1 - GUI message queue + Win32 window infrastructure\n");
    printf("================================================================\n");

    trace_init(ST_FALSE);

    /* ================================================================ */
    printf("\n--- Test group 1: gui_msg_queue_t nominal ---\n");
    /* ================================================================ */

    hQ = NULL;

    /* INTENT[INT-GUI-001 → TC-GUI-001 → REQ-GUI-009,REQ-GUI-010]:
     * create queue of capacity N: handle non-NULL */
    eRet = gui_msg_create(&hQ, 4);
    UC_TEST("[N] gui_msg_create(4) -> ST_NO_ERROR", eRet == ST_NO_ERROR);
    UC_TEST("[N] gui_msg_create: handle not NULL", hQ != NULL);

    /* INTENT[INT-GUI-001 → TC-GUI-002 → REQ-GUI-011]:
     * post one event, get it back intact — type preserved */
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

    /* INTENT[INT-GUI-001 → TC-GUI-003 → REQ-GUI-011]:
     * events dequeued in FIFO order, payload intact */
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

    /* INTENT[INT-GUI-001 → TC-GUI-004,TC-GUI-005 → REQ-GUI-011,REQ-GUI-012]:
     * fill queue to capacity then drain all events without loss */
    for (i = 0; i < 4; i++)
    {
        memset(&tEvt, 0, sizeof(tEvt));
        tEvt.eType            = GUI_EVT_SCROLL;
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

    /* INTENT[INT-GUI-001 → TC-GUI-006 → REQ-GUI-014]:
     * destroy releases resources and sets handle to NULL */
    eRet = gui_msg_destroy(&hQ);
    UC_TEST("[N] gui_msg_destroy -> ST_NO_ERROR", eRet == ST_NO_ERROR);
    UC_TEST("[N] gui_msg_destroy: handle set to NULL", hQ == NULL);

    /* ================================================================ */
    printf("\n--- Test group 2: gui_msg_queue_t robustness ---\n");
    /* ================================================================ */

    /* INTENT[INT-GUI-001 → TC-GUI-009 → REQ-GUI-009]:
     * NULL out-pointer rejected before any allocation */
    eRet = gui_msg_create(NULL, 4);
    UC_TEST("[R] gui_msg_create(NULL out) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-GUI-001 → TC-GUI-009 → REQ-GUI-010]:
     * zero capacity rejected — handle must stay NULL */
    hQ = NULL;
    eRet = gui_msg_create(&hQ, 0);
    UC_TEST("[R] gui_msg_create(capacity=0) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[R] gui_msg_create(0): handle stays NULL", hQ == NULL);

    /* Create a valid queue for remaining robustness tests */
    gui_msg_create(&hQ, 2);

    /* INTENT[INT-GUI-001 → TC-GUI-010 → REQ-GUI-011]:
     * NULL queue or NULL event rejected on post */
    memset(&tEvt, 0, sizeof(tEvt));
    tEvt.eType = GUI_EVT_KEY_DOWN;
    eRet = gui_msg_post(NULL, &tEvt);
    UC_TEST("[R] gui_msg_post(NULL queue) -> ST_ERROR",
            eRet == ST_ERROR);

    eRet = gui_msg_post(hQ, NULL);
    UC_TEST("[R] gui_msg_post(NULL event) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-GUI-001 → TC-GUI-010 → REQ-GUI-013]:
     * NULL queue or NULL output pointer rejected on get */
    eRet = gui_msg_get(NULL, &tGot, ST_FALSE);
    UC_TEST("[R] gui_msg_get(NULL queue) -> ST_ERROR",
            eRet == ST_ERROR);

    eRet = gui_msg_get(hQ, NULL, ST_FALSE);
    UC_TEST("[R] gui_msg_get(NULL event out) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-GUI-003 → TC-GUI-008 → REQ-GUI-013]:
     * non-blocking get on empty queue returns ST_ERROR immediately */
    eRet = gui_msg_get(hQ, &tGot, ST_FALSE);
    UC_TEST("[R] gui_msg_get on empty queue -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-GUI-002 → TC-GUI-007 → REQ-GUI-012]:
     * post beyond capacity returns ST_ERROR without blocking */
    memset(&tEvt, 0, sizeof(tEvt));
    tEvt.eType = GUI_EVT_PAINT;
    gui_msg_post(hQ, &tEvt);
    gui_msg_post(hQ, &tEvt);
    eRet = gui_msg_post(hQ, &tEvt);  /* 3rd post on capacity-2 queue */
    UC_TEST("[R] gui_msg_post beyond capacity -> ST_ERROR",
            eRet == ST_ERROR);

    gui_msg_destroy(&hQ);

    /* INTENT[INT-GUI-001 → TC-GUI-010 → REQ-GUI-014]:
     * destroy with NULL pphQueue or *pphQueue==NULL both rejected */
    eRet = gui_msg_destroy(NULL);
    UC_TEST("[R] gui_msg_destroy(NULL) -> ST_ERROR", eRet == ST_ERROR);

    hQ = NULL;
    eRet = gui_msg_destroy(&hQ);
    UC_TEST("[R] gui_msg_destroy(*pphQueue=NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    /* ================================================================ */
    printf("\n--- Test group 3: gui_init / gui_shutdown lifecycle ---\n");
    /* ================================================================ */

    /* INTENT[INT-GUI-004 → TC-GUI-011 → REQ-GUI-001]:
     * gui_init registers the Win32 WNDCLASS without error */
    eRet = gui_init();
    UC_TEST("[N] gui_init() -> ST_NO_ERROR", eRet == ST_NO_ERROR);

    /* INTENT[INT-GUI-004 → TC-GUI-011 → REQ-GUI-002]:
     * second gui_init call is idempotent — no double-registration */
    eRet = gui_init();
    UC_TEST("[N] gui_init() idempotent -> ST_NO_ERROR",
            eRet == ST_NO_ERROR);

    /* INTENT[INT-GUI-004 → TC-GUI-012 → REQ-GUI-007,REQ-GUI-008]:
     * gui_shutdown closes all resources cleanly */
    eRet = gui_shutdown();
    UC_TEST("[N] gui_shutdown() -> ST_NO_ERROR", eRet == ST_NO_ERROR);

    /* INTENT[INT-GUI-004 → TC-GUI-013 → REQ-GUI-007]:
     * gui_shutdown without prior init is a safe no-op */
    eRet = gui_shutdown();
    UC_TEST("[N] gui_shutdown() idempotent -> ST_NO_ERROR",
            eRet == ST_NO_ERROR);

    /* ================================================================ */
    printf("\n--- Test group 4: gui_* NULL-parameter robustness ---\n");
    /* ================================================================ */

    gui_init();

    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle = "UC3.1 Test";
    tDesc.eType   = GUI_WND_DIR;

    /* INTENT[INT-GUI-005 → TC-GUI-014 → REQ-GUI-003]:
     * gui_open_window rejects NULL ptDesc or NULL phWnd before side effects */
    hWnd = NULL;
    eRet = gui_open_window(NULL, &hWnd);
    UC_TEST("[R] gui_open_window(NULL desc) -> ST_ERROR",
            eRet == ST_ERROR);
    UC_TEST("[R] gui_open_window(NULL desc): hWnd stays NULL",
            hWnd == NULL);

    eRet = gui_open_window(&tDesc, NULL);
    UC_TEST("[R] gui_open_window(NULL phWnd) -> ST_ERROR",
            eRet == ST_ERROR);

    /* INTENT[INT-GUI-005 → TC-GUI-015 → REQ-GUI-005]:
     * gui_close_window, gui_invalidate, gui_get_size reject NULL handle */
    eRet = gui_close_window(NULL);
    UC_TEST("[R] gui_close_window(NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    eRet = gui_invalidate(NULL);
    UC_TEST("[R] gui_invalidate(NULL) -> ST_ERROR",
            eRet == ST_ERROR);

    iW = 0; iH = 0;
    eRet = gui_get_size(NULL, &iW, &iH);
    UC_TEST("[R] gui_get_size(NULL hWnd) -> ST_ERROR",
            eRet == ST_ERROR);

    gui_shutdown();

    /* ================================================================ */
    printf("\n--- Test group 5: window open / close (manual) ---\n");
    /* ================================================================ */

    /* INTENT[INT-GUI-004 → TC-GUI-016 → REQ-GUI-004]:
     * gui_open_window spawns a thread, window appears with dark background */
#ifdef ST_MANUAL_TEST
    gui_init();
    memset(&tDesc, 0, sizeof(tDesc));
    tDesc.szTitle  = "ST4Ever - UC3.1 Manual Test";
    tDesc.eType    = GUI_WND_DIR;
    tDesc.pfnEvent = NULL;
    tDesc.pUserCtx = NULL;
    hWnd = NULL;
    gui_open_window(&tDesc, &hWnd);
    platform_sleep_ms(500);
    TEST_MANUAL("[S] TC-GUI-016 window open with dark background",
                "Is a dark window titled 'ST4Ever - UC3.1 Manual Test' "
                "visible?");

    /* INTENT[INT-GUI-004 → TC-GUI-017 → REQ-GUI-006]:
     * gui_close_window posts WM_CLOSE, thread exits, handle freed */
    TEST_MANUAL("[S] TC-GUI-017 title bar shows correct title",
                "Does the title bar read 'ST4Ever - UC3.1 Manual Test'?");
    gui_close_window(hWnd);
    hWnd = NULL;

    /* INTENT[INT-GUI-005 → TC-GUI-018 → REQ-GUI-015]:
     * OS close button (X) fires WM_DESTROY -> GUI_EVT_CLOSE callback */
    TEST_MANUAL("[S] TC-GUI-018 gui_close_window: window disappears cleanly",
                "Did the window disappear from the taskbar?");
    gui_shutdown();
#else
    TEST_SKIP("[S] TC-GUI-016 gui_open_window() - window visible (run make manual)");
    TEST_SKIP("[S] TC-GUI-017 window: dark background, title bar visible");
    TEST_SKIP("[S] TC-GUI-018 gui_close_window() - window closes cleanly");
#endif

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
