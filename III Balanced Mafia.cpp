// MafiaUzi.asi - GTA III PC v1.0
// Troca a arma (shotgun -> micro Uzi) dos mafiosos que o jogo cria soltos pela cidade.
//
// Como funciona (confirmado no gta3.exe 1.0):
//  - CPopulation::AddPed (0x4F5280): pedTypes 7..15 (gangues) usam o mesmo caso, que le a
//    arma da tabela de gangues (CGangs) e chama CPed::GiveWeapon(arma, 25001).
//  - Tabela de gangues: base 0x6EDF78, 16 bytes por gangue; arma 1 em +8 e arma 2 em +12.
//  - A Mafia e a gangue 0 (PEDTYPE_GANG1 = 7).
//  - A municao dos pedestres de gangue e fixa em 25001 (0x61A9) para qualquer arma, entao
//    ao trocar a arma a Uzi nasce com o mesmo total de municao que a shotgun tinha.
//  - Pedestres de missao recebem a arma pelo script e nao sao afetados.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const DWORD GAME_VERSION_ADDR = 0x601048;
static const DWORD GAME_VERSION_1_0  = 0x3A83126F;
static const DWORD GANG_WEAPON_BASE  = 0x6EDF80;   // arma 1 da gangue 0
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

    if (!GetPrivateProfileIntA("Main", "Enable", 1, gIni)) { Log("Desativado no .ini"); return 0; }

    // So roda no GTA III 1.0: confere a versao e os bytes das 2 leituras da tabela de gangues
    if (IsBadReadPtr((const void*)GAME_VERSION_ADDR, 4) || *(const DWORD*)GAME_VERSION_ADDR != GAME_VERSION_1_0)
    { Log("Nao e o GTA III 1.0 - mod desativado"); return 0; }

    const unsigned char sig1[] = { 0x8B, 0x86, 0x10, 0xDF, 0x6E, 0x00 }; // mov eax,[esi+6EDF10h]  @4F5605
    const unsigned char sig2[] = { 0x8B, 0x82, 0x14, 0xDF, 0x6E, 0x00 }; // mov eax,[edx+6EDF14h]  @4F561F
    if (!CodeMatches(0x4F5605, sig1, sizeof sig1) || !CodeMatches(0x4F561F, sig2, sizeof sig2))
    { Log("Codigo do jogo diferente do esperado - mod desativado"); return 0; }

    int gang   = GetPrivateProfileIntA("Main", "Gang", 0, gIni);          // 0 = Mafia
    int from   = GetPrivateProfileIntA("Main", "FromWeapon", 4, gIni);    // 4 = Shotgun
    int to     = GetPrivateProfileIntA("Main", "ToWeapon", 3, gIni);      // 3 = Uzi
    if (gang < 0 || gang > 8) gang = 0;
    Log("Ativo: gangue=%d, arma %d -> %d", gang, from, to);
    if (from == to) return 0;

    volatile int* w[2];
    w[0] = (volatile int*)(GANG_WEAPON_BASE + GANG_STRIDE * gang);
    w[1] = (volatile int*)(GANG_WEAPON_BASE + GANG_STRIDE * gang + 4);

    // A tabela e preenchida pelo script (e recarregada ao carregar um save),
    // entao verificamos de tempos em tempos e trocamos sempre que aparecer a arma original.
    while (true)
    {
        Sleep(200);
        for (int i = 0; i < 2; i++)
        {
            if (*w[i] == from)
            {
                *w[i] = to;
                Log("Arma %d da gangue %d trocada: %d -> %d", i + 1, gang, from, to);
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
