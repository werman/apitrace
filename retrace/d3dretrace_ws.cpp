/**************************************************************************
 *
 * Copyright 2012 Jose Fonseca
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/


#include <assert.h>

#include "d3dretrace.hpp"

#if !defined(APITRACE_DXVK_NATIVE)
#include "ws_win32.hpp"
#endif


namespace d3dretrace {


HWND
createWindow(int width, int height)
{
#if defined(APITRACE_DXVK_NATIVE)
    (void)width;
    (void)height;
    return NULL;
#else
    HWND hWnd;

    hWnd = ws::createWindow("d3dretrace", width, height);

    ws::showWindow(hWnd);

    return hWnd;
#endif
}


#if !defined(APITRACE_DXVK_NATIVE)
typedef std::map<HWND, HWND> HWND_MAP;
static HWND_MAP g_hWndMap;
#endif


HWND
createWindow(HWND hWnd, int width, int height)
{
#if defined(APITRACE_DXVK_NATIVE)
    (void)hWnd;
    (void)width;
    (void)height;
    return NULL;
#else
    HWND_MAP::iterator it;
    it = g_hWndMap.find(hWnd);
    if (it == g_hWndMap.end()) {
        // Create a new window
        hWnd = createWindow(width, height);
        g_hWndMap[hWnd] = hWnd;
    } else {
        // Reuse the existing window
        hWnd = it->second;
        ws::resizeWindow(hWnd, width, height);
    }
    return hWnd;
#endif
}


void
resizeWindow(HWND hWnd, int width, int height)
{
#if defined(APITRACE_DXVK_NATIVE)
    (void)hWnd;
    (void)width;
    (void)height;
#else
    ws::resizeWindow(hWnd, width, height);
#endif
}


bool
processEvents(void)
{
#if defined(APITRACE_DXVK_NATIVE)
    return false;
#else
    return ws::processEvents();
#endif
}


} /* namespace d3dretrace */
