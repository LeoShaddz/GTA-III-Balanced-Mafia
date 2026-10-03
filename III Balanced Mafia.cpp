// MafiaUzi.asi - GTA III PC v1.0
// Replaces the weapon (shotgun -> Micro Uzi) of Mafia members spawned freely around the city.
//
// How it works (confirmed in gta3.exe 1.0):
//  - CPopulation::AddPed (0x4F5280): pedTypes 7..15 (gangs) use the same case, which reads the
//    weapon from the gang table (CGangs) and calls CPed::GiveWeapon(weapon, 25001).
//  - Gang table: base 0x6EDF78, 16 bytes per gang; weapon 1 at +8 and weapon 2 at +12.
//  - The Mafia is gang 0 (PEDTYPE_GANG1 = 7).
//  - Gang pedestrian ammunition is fixed at 25001 (0x61A9) for any weapon, so
//    when the weapon is replaced, the Uzi spawns with the same total ammunition as the shotgun.
//  - Mission pedestrians receive their weapons through the script and are not affected.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const DWORD GAME_VERSION_ADDR = 0x601048;
static const DWORD GAME_VERSION_1_0  = 0x3A83126F;
static const DWORD GANG_WEAPON_BASE  = 0x6EDF80;   // Weapon 1 of gang 0
static const DWORD GANG_STRIDE       = 0x10;

static HMODULE hSelf = NULL;
static char gIni[MAX_PATH];

static bool gLog = false;
static void Log(const char* fmt, ...)
{
    if (!gLog) return;
    char path[MAX_PATH];
    if (!GetModuleFileNameA(hSelf, path, MAX_PATH)) return;
    char* dot = strrchr(path, '.');
    if (dot) strcpy(dot, ".log"); else strcat(path, ".log");
    FILE* f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "[%lu] ", GetTickCount());
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

static bool CodeMatches(DWORD addr, const unsigned char* sig, size_t n)
{
    if (IsBadReadPtr((const void*)addr, n)) return false;
    return memcmp((const void*)addr, sig, n) == 0;
}

static DWORD WINAPI Thread(LPVOID)
{
    gLog = GetPrivateProfileIntA("Main", "Log", 0, gIni) != 0;

    if (!GetPrivateProfileIntA("Main", "Enable", 1, gIni)) { Log("Disabled in .ini"); return 0; }

    // Only runs on GTA III 1.0: checks the version and the bytes of the 2 gang table reads
    if (IsBadReadPtr((const void*)GAME_VERSION_ADDR, 4) || *(const DWORD*)GAME_VERSION_ADDR != GAME_VERSION_1_0)
    { Log("Not GTA III 1.0 - mod disabled"); return 0; }

    const unsigned char sig1[] = { 0x8B, 0x86, 0x10, 0xDF, 0x6E, 0x00 }; // mov eax,[esi+6EDF10h]  @4F5605
    const unsigned char sig2[] = { 0x8B, 0x82, 0x14, 0xDF, 0x6E, 0x00 }; // mov eax,[edx+6EDF14h]  @4F561F
    if (!CodeMatches(0x4F5605, sig1, sizeof sig1) || !CodeMatches(0x4F561F, sig2, sizeof sig2))
    { Log("Game code differs from the expected version - mod disabled"); return 0; }

    int gang   = GetPrivateProfileIntA("Main", "Gang", 0, gIni);          // 0 = Mafia
    int from   = GetPrivateProfileIntA("Main", "FromWeapon", 4, gIni);    // 4 = Shotgun
    int to     = GetPrivateProfileIntA("Main", "ToWeapon", 3, gIni);      // 3 = Uzi
    if (gang < 0 || gang > 8) gang = 0;
    Log("Active: gang=%d, weapon %d -> %d", gang, from, to);
    if (from == to) return 0;

    volatile int* w[2];
    w[0] = (volatile int*)(GANG_WEAPON_BASE + GANG_STRIDE * gang);
    w[1] = (volatile int*)(GANG_WEAPON_BASE + GANG_STRIDE * gang + 4);

    // The table is populated by the script (and reloaded when a save is loaded),
    // so we check periodically and replace the original weapon whenever it appears.
    while (true)
    {
        Sleep(200);
        for (int i = 0; i < 2; i++)
        {
            if (*w[i] == from)
            {
                *w[i] = to;
                Log("Weapon %d of gang %d changed: %d -> %d", i + 1, gang, from, to);
            }
        }
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        hSelf = hModule;
        GetModuleFileNameA(hModule, gIni, MAX_PATH);
        char* dot = strrchr(gIni, '.');
        if (dot) strcpy(dot, ".ini"); else strcat(gIni, ".ini");
        CreateThread(0, 0, Thread, NULL, 0, NULL);
    }
    return TRUE;
}
