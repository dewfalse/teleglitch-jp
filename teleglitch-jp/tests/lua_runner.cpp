#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdint>

// Run Lua 5.2 tests with the exact 32-bit Lua DLL shipped with this game.
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    HMODULE lib = LoadLibraryW(L".\\lua52.dll");
    if (!lib) { std::printf("Lua DLL load failed: %lu\n", GetLastError()); return 2; }
    auto create = reinterpret_cast<void* (__cdecl*)()>(GetProcAddress(lib, "luaL_newstate"));
    auto open = reinterpret_cast<void (__cdecl*)(void*)>(GetProcAddress(lib, "luaL_openlibs"));
    auto load = reinterpret_cast<int (__cdecl*)(void*, const char*, const char*)>(GetProcAddress(lib, "luaL_loadfilex"));
    auto call = reinterpret_cast<int (__cdecl*)(void*, int, int, int, int, void*)>(GetProcAddress(lib, "lua_pcallk"));
    auto text = reinterpret_cast<const char* (__cdecl*)(void*, int, size_t*)>(GetProcAddress(lib, "lua_tolstring"));
    auto close = reinterpret_cast<void (__cdecl*)(void*)>(GetProcAddress(lib, "lua_close"));
    if (!create || !open || !load || !call || !text || !close) return 2;
    void* state = create();
    if (!state) return 2;
    open(state);
    int error = load(state, argv[1], nullptr);
    if (!error) error = call(state, 0, -1, 0, 0, nullptr);
    if (error) std::fprintf(stderr, "%s\n", text(state, -1, nullptr));
    close(state);
    FreeLibrary(lib);
    return error ? 1 : 0;
}
