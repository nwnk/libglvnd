/*
 * Copyright (c) 2013, NVIDIA CORPORATION.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * unaltered in all copies or substantial portions of the Materials.
 * Any additions, deletions, or changes to the original source files
 * must be clearly indicated in accompanying documentation.
 *
 * If only executable code is distributed, then the accompanying
 * documentation must state that "this software is based in part on the
 * work of the Khronos Group."
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 */

#include <X11/Xlibint.h>

#include <pthread.h>
#include <dlfcn.h>

#if defined(HASH_DEBUG)
# include <stdio.h>
#endif

#include "libglxmapping.h"
#include "libglxnoop.h"
#include "libglxthread.h"
#include "trace.h"

#include "lkdhash.h"

#define _GNU_SOURCE 1
#include <assert.h>
#include <stdio.h>

/*
 * __glXVendorScreenHash is a hash table which maps a Display+screen to a vendor.
 * Look up this mapping from the X server once, the first time a unique
 * Display+screen pair is seen.
 */
typedef struct {
    Display *dpy;
    int screen;
} __GLXvendorScreenHashKey;


typedef struct __GLXvendorScreenHashRec {
    __GLXvendorScreenHashKey key;
    __GLXvendorInfo *vendor;
    // XXX for performance reasons we may want to stash the dispatch tables here
    // as well
    UT_hash_handle hh;
} __GLXvendorScreenHash;

static DEFINE_INITIALIZED_LKDHASH(__GLXvendorScreenHash, __glXVendorScreenHash);

/*
 * This function queries each loaded vendor to determine if there is
 * a vendor-implemented dispatch function. The dispatch function
 * uses the vendor <-> API library ABI to determine the screen given
 * the parameters of the function and dispatch to the correct vendor's
 * implementation.
 */
__GLXextFuncPtr __glXGetGLXDispatchAddress(const GLubyte *procName)
{
    return NULL; // TODO;
}

__GLXvendorInfo *__glXLookupVendorByName(const char *vendorName)
{
    return NULL;
}

__GLXvendorInfo *__glXLookupVendorByScreen(Display *dpy, const int screen)
{
    __GLXvendorInfo *vendor = NULL;
    __GLXvendorScreenHash *pEntry = NULL;
    __GLXvendorScreenHashKey key;

    if (screen < 0) {
        return NULL;
    }

    memset(&key, 0, sizeof(key));

    key.dpy = dpy;
    key.screen = screen;

    LKDHASH_RDLOCK(__glXPthreadFuncs, __glXVendorScreenHash);

    HASH_FIND(hh, _LH(__glXVendorScreenHash), &key,
              sizeof(key), pEntry);
    if (pEntry) {
        vendor = pEntry->vendor;
    }

    LKDHASH_UNLOCK(__glXPthreadFuncs, __glXVendorScreenHash);

    if (!pEntry) {
        /*
         * If we have specified a vendor library, use that. Otherwise,
         * try to lookup the vendor based on the current screen.
         */
        const char *preloadedVendorName = getenv("__GLX_VENDOR_LIBRARY_NAME");
        char *queriedVendorName;

        assert(!vendor);

        if (preloadedVendorName) {
            vendor = __glXLookupVendorByName(preloadedVendorName);
        }

        if (!vendor) {
            queriedVendorName = NULL; // TODO: query X somehow for the name
            vendor = __glXLookupVendorByName(queriedVendorName);
            Xfree(queriedVendorName);
        }

        if (!vendor) {
            assert(!"Missing vendor library!");
        }

        LKDHASH_WRLOCK(__glXPthreadFuncs, __glXVendorScreenHash);

        HASH_FIND(hh, _LH(__glXVendorScreenHash), &key, sizeof(key), pEntry);

        if (!pEntry) {
            pEntry = malloc(sizeof(*pEntry));
            if (!pEntry) {
                return NULL;
            }

            pEntry->key.dpy = dpy;
            pEntry->key.screen = screen;
            pEntry->vendor = vendor;
            HASH_ADD(hh, _LH(__glXVendorScreenHash), key,
                     sizeof(__GLXvendorScreenHashKey), pEntry);
        } else {
            /* Some other thread already added a vendor */
            vendor = pEntry->vendor;
        }

        LKDHASH_UNLOCK(__glXPthreadFuncs, __glXVendorScreenHash);
    }

    DBG_PRINTF(10, "Found vendor \"%s\" for screen %d\n",
               vendor->name, screen);

    return vendor;
}

const __GLXdispatchTableStatic *__glXGetStaticDispatch(Display *dpy, const int screen)
{
    __GLXvendorInfo *vendor = __glXLookupVendorByScreen(dpy, screen);

    if (vendor) {
        assert(vendor->staticDispatch);
        return vendor->staticDispatch;
    } else {
        return __glXDispatchNoopPtr;
    }
}

void *__glXGetGLDispatch(Display *dpy, const int screen)
{
    __GLXvendorInfo *vendor = __glXLookupVendorByScreen(dpy, screen);

    if (vendor) {
        assert(vendor->glDispatch);
        return vendor->glDispatch;
    } else {
        return NULL;
    }
}

__GLXdispatchTableDynamic *__glXGetDynDispatch(Display *dpy, const int screen)
{
    __GLXvendorInfo *vendor = __glXLookupVendorByScreen(dpy, screen);

    if (vendor) {
        assert(vendor->dynDispatch);
        return vendor->dynDispatch;
    } else {
        return NULL;
    }
}

static void AddScreenPointerMapping(void *ptr, int screen)
{
    // TODO
}


static void RemoveScreenPointerMapping(void *ptr, int screen)
{
    // TODO
}


static int ScreenFromPointer(void *ptr)
{
    // TODO
    return -1;
}


void __glXAddScreenContextMapping(GLXContext context, int screen)
{
    AddScreenPointerMapping(context, screen);
}


void __glXRemoveScreenContextMapping(GLXContext context, int screen)
{
    RemoveScreenPointerMapping(context, screen);
}


int __glXScreenFromContext(GLXContext context)
{
    return ScreenFromPointer(context);
}


void __glXAddScreenFBConfigMapping(GLXFBConfig config, int screen)
{
    AddScreenPointerMapping(config, screen);
}


void __glXRemoveScreenFBConfigMapping(GLXFBConfig config, int screen)
{
    RemoveScreenPointerMapping(config, screen);
}


int __glXScreenFromFBConfig(GLXFBConfig config)
{
    return ScreenFromPointer(config);
}




/****************************************************************************/
static void AddScreenXIDMapping(XID xid, int screen)
{
    // TODO
}


static void RemoveScreenXIDMapping(XID xid, int screen)
{
    // TODO
}


static int ScreenFromXID(Display *dpy, XID xid)
{
    return -1; // TODO
}


void __glXAddScreenDrawableMapping(GLXDrawable drawable, int screen)
{
    AddScreenXIDMapping(drawable, screen);
}


void __glXRemoveScreenDrawableMapping(GLXDrawable drawable, int screen)
{
    RemoveScreenXIDMapping(drawable, screen);
}


int __glXScreenFromDrawable(Display *dpy, GLXDrawable drawable)
{
    return ScreenFromXID(dpy, drawable);
}