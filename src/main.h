/*
 * main.h - ST4Ever main application entry point
 *
 * declares the ST4Ever_main() function
 */

#ifndef ST_MAIN
#define ST_MAIN

#include "common.h"

/* ------------------------------------------------------------------
 * ST4Ever Application Context
 * ------------------------------------------------------------------ */

typedef struct ST4Ever_context_s
{
    st_u32_t    ulMagic;                   /* Magic ST4Ever OO-like tag */
    st_object_t eObject;                   /* Object type for tests     */
    
    st_bool_t   bTraceAtStart;             /* Manage --trace option     */
    char        szScriptFile[ST_MAX_PATH]; /* Manage --script option    */
    st_u64_t    ptWinConsoleCtx;           /* win_console context       */
    st_u64_t    ptTraceCtx;                /* Trace Context             */
    st_u64_t    ptGUICtx;                  /* GUI Context               */
    st_u64_t    ptSTMachineCtx;            /* ST Machine Context        */
    st_u64_t    ptSTLoadCtx;               /* ST Machine Load Context   */
    st_u64_t    ptSTExecCtx;               /* ST Machine Exec Context   */
    st_u64_t    ptConsoleCtx;              /* Console Context           */

} ST4Ever_context_t;


/*
 * ST4Ever_init() - Initialization of ST4Ever application.
 *
 * This function manages the command line options and initialise the
 * context of all sub-modules. A test application can retrieve the 
 * context values to get access to internal global variables of each
 * module (e.g. gui.c, line.c, exec.c, ...) 
 *
 * Parameters:
 *   argc [in] : number of commang-line arguments
 *   argv [in] : the array of command line arguments
 * 
 * Returns:
 *   Value of the global ST4Ever_context_t structure pointer on success.
 *   ST_ERROR, if an error occurs
 * 
 */
st_u64_t ST4Ever_init(int argc, char *argv[]);

/*
 * ST4Ever_run() - Run the user interactive console of ST4Ever
 *
 * This function is split from init and shutdown to allow a test
 * application to directly interact with the sub-modules after
 * ST4Ever init and before the proper shutdown, in place of this
 * function.
 *
 * Parameters:
 *   None
 * 
 * Returns:
 *   None
 * 
 */
void       ST4Ever_run();

/*
 * ST4Ever_shutdown() - Quit properly the ST4Ever Application
 *
 * This function cleans-up the allocated structures in reverse order
 * from the init function
 *
 * Parameters:
 *   None
 * 
 * Returns:
 *   ST_NO_ERROR, in case of success or ST_ERROR otherwise
 * 
 */
st_error_t ST4Ever_shutdown();

#endif /* ST_MAIN */

