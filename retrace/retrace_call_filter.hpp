#pragma once

#include <cstring>

namespace retrace {

inline bool isD3DCursorCall(const char *name)
{
    static const char *const interfaces[] = {
        "IDirect3DDevice8::", "IDirect3DDevice9::", "IDirect3DDevice9Ex::"
    };
    for (const char *interfaceName : interfaces) {
        const size_t length = std::strlen(interfaceName);
        if (std::strncmp(name, interfaceName, length) == 0) {
            const char *method = name + length;
            return std::strcmp(method, "SetCursorProperties") == 0 ||
                   std::strcmp(method, "SetCursorPosition") == 0 ||
                   std::strcmp(method, "ShowCursor") == 0;
        }
    }
    return false;
}

} // namespace retrace
