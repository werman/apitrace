// SPDX-License-Identifier: MIT
// Android native replay frame markers using DXVK Vulkan interop.
#include "d3dretrace.hpp"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

namespace d3dretrace {
namespace {
// Prefix of DXVK's ID3D9VkInteropDevice ABI. Vulkan dispatchable handles are
// opaque pointers; keeping these declarations local avoids D3D8/9 header clashes.
struct InteropDevice : IUnknown {
    virtual void STDMETHODCALLTYPE GetVulkanHandles(void **, void **, void **) = 0;
    virtual void STDMETHODCALLTYPE GetSubmissionQueue(void **, uint32_t *, uint32_t *) = 0;
    virtual void STDMETHODCALLTYPE TransitionTextureLayout(void *, const void *, int, int) = 0;
    virtual void STDMETHODCALLTYPE FlushRenderingCommands() = 0;
    virtual void STDMETHODCALLTYPE LockSubmissionQueue() = 0;
    virtual void STDMETHODCALLTYPE ReleaseSubmissionQueue() = 0;
};
InteropDevice *interop;
IUnknown *timingDevice;
void *instance, *device, *queue;
uint32_t family;
int64_t frame;
bool active;
using SetFrame = void (*)(int64_t);
using Marker = void (*)(void *, void *, void *, uint32_t, int);
using Finish = void (*)();
SetFrame setFrame;
Marker marker;
Finish finish;

void fail(const char *message) {
    fprintf(stderr, "driver-ci frame timing: %s\n", message);
    abort();
}

void emit(bool begin) {
    interop->FlushRenderingCommands();
    interop->LockSubmissionQueue();
    setFrame(frame);
    marker(instance, device, queue, family, begin);
    interop->ReleaseSubmissionQueue();
}

void shutdown() {
    if (!interop) return;
    // Do not close an unfinished final frame: it has no Present boundary.
    interop->FlushRenderingCommands();
    interop->LockSubmissionQueue();
    finish();
    interop->ReleaseSubmissionQueue();
    interop->Release();
    interop = nullptr;
}
}

void startFrameTiming(IUnknown *d3dDevice) {
    const char *enabled = getenv("DRIVER_CI_FRAME_MARKERS");
    if (!enabled || enabled[0] != '1') return;
    if (d3dDevice) {
        if (interop) fail("multiple D3D devices are unsupported");
        timingDevice = d3dDevice;
        static const GUID iid = {0x2eaa4b89, 0x0107, 0x4bdb,
            {0x87, 0xf7, 0x0f, 0x54, 0x1c, 0x49, 0x3c, 0xe0}};
        if (FAILED(d3dDevice->QueryInterface(iid, reinterpret_cast<void **>(&interop))))
            fail("DXVK Vulkan interop unavailable (D3D8 requires the interop forwarding build)");
        void *shim = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        setFrame = shim ? reinterpret_cast<SetFrame>(dlsym(shim, "rdci_set_frame")) : nullptr;
        marker = shim ? reinterpret_cast<Marker>(dlsym(shim, "rdci_frame_marker")) : nullptr;
        finish = shim ? reinterpret_cast<Finish>(dlsym(shim, "rdci_finish_markers")) : nullptr;
        if (!setFrame || !marker || !finish) fail("replay marker shim unavailable");
        interop->GetVulkanHandles(&instance, nullptr, &device);
        interop->GetSubmissionQueue(&queue, nullptr, &family);
        atexit(shutdown);
        return; // Initialization is unmarked; timing starts after the first Present.
    }
    if (!interop) fail("Present without an initialized timing device");
    if (active) fail("nested frame start");
    frame = retrace::frameNo;
    emit(true);
    active = true;
}

void endFrameTiming() {
    if (!interop) return;
    if (!active) {
        interop->FlushRenderingCommands();
        return;
    }
    emit(false);
    active = false;

}
void releaseFrameTiming(IUnknown *d3dDevice) {
    if (d3dDevice != timingDevice) return;
    shutdown();
    timingDevice = nullptr;
    active = false;
}

}
