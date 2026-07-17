/*
 * use_case_msg_queue.c - UC3.1 Validation: GUI message queue + Win32 window
 *
 * Traceability chain per INTENT block:
 *   INTENT[INT-xxx-NNN → TC-xxx-NNN → REQ-xxx-NNN -> UFR-xxx-NNN]
 *
 * NOTE: The GUI message queue is developped but not used yet in ST4Ever
 *       It was foreseen for communication from the main console to the
 *       GUI windows open from the console (e.g. trace, dir, edit_hex, ...)
 * 
 * This use_case_msg_queue.c is temporarily managed out of the other use_cases_nn.c
 * series of test cases.
 * 
 * TEST MATRIX - UC3.1:
 *   [N] Nominal    : 12 tests - gui_msg_queue_t full lifecycle (incl.
 *                               loop assertions), gui_init/shutdown
 *   [R] Robustness : 11 tests - NULL params, capacity 0, queue full,
 *                               queue empty, NULL window handles
 *   [S] Skipped    :  0 tests - 
 *
 * Test groups:
 *   Group 1: gui_msg_queue_t nominal  (create, post, get, FIFO,
 *             fill/drain to capacity, destroy)
 *   Group 2: gui_msg_queue_t robustness (NULL params, capacity 0,
 *             queue full, queue empty)
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
    st_error_t       eRet;
    int              i;

    printf("\n");
    printf("================================================================\n");
    printf(" UC3.1 - GUI message queue \n");
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
