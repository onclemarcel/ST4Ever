/* utils.c - helpers to common functions to the ST4Ever application 
 * 
 * see common.h for function prototypes and common declarations
 * 
 */
#include "common.h"
#include "trace.h"
#include "dir.h"

/* Return object size */
#define GET_OBJ_SIZE(type, size)    \
    do { \
        switch(type)                \
        {                           \
            case ST_DIR_NODE_T:     \
                size = sizeof(dir_node_t);          \
                break;              \
            default:                \
                size = 0;           \
                break;              \
        }                           \
    } while(0)


 /* ==================================================================
 * Memory helpers - tracks allocated memory
 * ================================================================== */

void* mem_alloc(st_object_t type, st_u32_t *sum)
{
    void *p;
    st_bool_t   isRegistered;
    st_u32_t    uiSize = 0;

    // No LOG_TRACE - R22: called too often - traces are kept by callers

    /* -- [MEM]1. Reject unregistered objects -- */
    IS_OBJ(type, isRegistered);
    if (!isRegistered)
    {
        LOG_ERROR("Cannot allocate unregistered type (%02X)", type);
        return NULL;
    }

    /* -- [MEM]2. Allocate given bytes in memory and initialize object -- */
    GET_OBJ_SIZE(type, uiSize);
    if (uiSize == 0)
    {
        LOG_ERROR("Unable to retrieve object size (%02X)", type);
        return NULL;
    }

    p = malloc(uiSize);
    if (p == NULL)
    {
        LOG_ERROR("malloc failed");
        return NULL;
    }
    
    memset(p, 0, uiSize);
    st_obj_generic_t* ptGen = (st_obj_generic_t*)p;
    ptGen->ulMagic = 0xCAFEDECA;
    ptGen->eObject = type;
    
    /* -- [MEM]3. Add allocated size to the given sum value (if given) -- */
    if (sum != NULL)
    {
        *sum = *sum + uiSize;
    }
    
    return p;
}

void mem_free(void* p, st_u32_t *sum)
{
    st_bool_t   isRegistered;
    st_object_t eType;
    st_u32_t    uiSize = 0;

    // No LOG_TRACE - R22: called too often - traces are kept by callers

    /* -- [MEM]4. Do nothing on NULL pointers -- */
    if (p == NULL) return;
    
    /* -- [MEM]5. Free but log an error on unrecognized objects -- */
    st_obj_generic_t* ptGen = p;
    if (ptGen->ulMagic != 0xCAFEDECA)
    {
        LOG_ERROR("Freeing an unknown pointer (%p (ulMagic=%d))", 
                                                  (void*)p, ptGen->ulMagic);
        free(p);
        return;
    }

    /* -- [MEM]6. Free but log an error on unregistered types -- */
    eType = ptGen->eObject;
    IS_OBJ(eType, isRegistered);
    if (!isRegistered)
    {
        LOG_ERROR("Freeing an unregistered type (%02X)", eType);
        free(p);
        return;
    }

    free(p);

    /* -- [MEM]7. Add free size to the given sum value (if given) -- */
    GET_OBJ_SIZE(eType, uiSize);
    if (sum != NULL)
    {
        *sum = *sum + uiSize;
    }
}
