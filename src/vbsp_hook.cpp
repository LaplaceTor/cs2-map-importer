#include <windows.h>
#include <stdio.h>
#include "minhook/MinHook.h"

// Original function pointer type for SubdivideFaceList
typedef void (__cdecl *SubdivideFaceList_t)(void** pFaceList);

// Pointer to the original SubdivideFaceList function (needed by MinHook)
SubdivideFaceList_t fpSubdivideFaceList = NULL;

// Hook function
void __cdecl Hooked_SubdivideFaceList(void** pFaceList)
{
    // Do absolutely nothing! This skips face subdivision!
}

// Global variable patch addresses
#define NOSUBDIV_ADDR 0x0091C528
#define NOTJUNC_ADDR 0x0091C538
#define SUBDIVIDE_FACE_LIST_ADDR 0x004164A0

void ApplyPatches()
{
    DWORD oldProtect;

    // 1. Patch nosubdiv global variable to 1
    if (VirtualProtect((LPVOID)NOSUBDIV_ADDR, sizeof(int), PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        *(int*)(NOSUBDIV_ADDR) = 1;
        VirtualProtect((LPVOID)NOSUBDIV_ADDR, sizeof(int), oldProtect, &oldProtect);
    }

    // 2. Patch notjunc global variable to 1 (skips T-junction repair which can cause re-triangulation of bad start vertices)
    if (VirtualProtect((LPVOID)NOTJUNC_ADDR, sizeof(int), PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        *(int*)(NOTJUNC_ADDR) = 1;
        VirtualProtect((LPVOID)NOTJUNC_ADDR, sizeof(int), oldProtect, &oldProtect);
    }

    // 3. Initialize MinHook and hook SubdivideFaceList
    if (MH_Initialize() == MH_OK)
    {
        if (MH_CreateHook((LPVOID)SUBDIVIDE_FACE_LIST_ADDR, (LPVOID)&Hooked_SubdivideFaceList, (LPVOID*)&fpSubdivideFaceList) == MH_OK)
        {
            MH_EnableHook((LPVOID)SUBDIVIDE_FACE_LIST_ADDR);
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        ApplyPatches();
        break;
    case DLL_PROCESS_DETACH:
        MH_DisableHook((LPVOID)SUBDIVIDE_FACE_LIST_ADDR);
        MH_Uninitialize();
        break;
    }
    return TRUE;
}
