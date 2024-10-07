#include <Windows.h>
#include <vector>
#include <iostream>
#include <stdio.h>
#include <inttypes.h>
#include <thread>
#include <psapi.h> // Include Psapi.h for GetModuleInformation
#include <Zydis/Zydis.h>

#include "lazy_importer.hpp"
#include "finder_funcs.hpp"

using namespace cgame;
using namespace cgame::ballistics;
using namespace cgame::ballistics::telecontrol;
using namespace cgame::ballistics::telecontrol::gameui;
using namespace localplayer;
using namespace unit;

namespace offsets
{
    namespace cgame_offsets
    {
        constexpr uintptr_t ballistics_offset = 0x3e0;
        namespace ballistic_offsets
        {
            constexpr uintptr_t round_caliber_offset = 0x1d90;
            constexpr uintptr_t round_length_offset = 0x2000;
            constexpr uintptr_t round_mass_offset = 0x1d8c;
            constexpr uintptr_t round_velocity_offset = 0x1d80;
            constexpr uintptr_t telecontrol_offset = 0xae0;
            namespace telecontrol_offsets
            {
                constexpr uintptr_t gameui_offset = 0x658;
                namespace gameui_offsets
                {
                    constexpr uintptr_t mousepos_offset = 0xba8;
                }
            }
        }
    }
    namespace localplayer_offsets
    {
        constexpr uintptr_t localunit_offset = 0x700;
    }
    namespace unit_offsets
    {
        constexpr uintptr_t allowBailout = 0xb48;
        constexpr uintptr_t artillery = 0x1600;
        constexpr uintptr_t bombDelayExplosion = 0x9b0;
        constexpr uintptr_t briefMalfunctionState = 0x988;
        constexpr uintptr_t brokenTurretDriveJammedTime = 0x6e8;
        constexpr uintptr_t brokenTurretDriveMult = 0x738;
        constexpr uintptr_t brokenTurretDriveSpeed = 0x710;
        constexpr uintptr_t curNightVisionMode = 0x808;
        constexpr uintptr_t delayWithFlightTime = 0x9d8;
        constexpr uintptr_t dummyForDeathInfo = 0xa60;
        constexpr uintptr_t dummyForUnitFlags = 0x1270;
        constexpr uintptr_t dummyVarForCrewLayout = 0x838;
        constexpr uintptr_t dummyVarForMissionAddText = 0x860;
        constexpr uintptr_t enableGunners = 0x1210;
        constexpr uintptr_t extinguishAssistant = 0xc90;
        constexpr uintptr_t extinguishAssistee = 0xc60;
        constexpr uintptr_t farthestExitZoneId = 0x1310;
        constexpr uintptr_t hasModuleEffectsToRepair = 0x788;
        constexpr uintptr_t invulnerable = 0x11e8;
        constexpr uintptr_t isAlternativeShotFreq = 0x2c0;
        constexpr uintptr_t isBreechDamaged = 0x760;
        constexpr uintptr_t isNeedExtinguishHelp = 0x1ca0;
        constexpr uintptr_t isNeedRepairHelp = 0x1c70;
        constexpr uintptr_t isUnderwater = 0x1f70;
        constexpr uintptr_t killer = 0x1ba0;
        constexpr uintptr_t lastContactFrom = 0xa98;
        constexpr uintptr_t lastContactTime = 0xaf8;
        constexpr uintptr_t lastContactTimeRel = 0xb20;
        constexpr uintptr_t lastContactTo = 0xac8;
        constexpr uintptr_t lowRateUnitFlags = 0x12a0;
        constexpr uintptr_t maskingFactor = 0x1d28;
        constexpr uintptr_t moduleEffectsRepairAtTime = 0x7b0;
        constexpr uintptr_t nextUseArtilleryTime = 0x1a80;
        constexpr uintptr_t prepareExtinguishAssistantTime = 0x1a10;
        constexpr uintptr_t prepareRepairAssistantTime = 0x19e8;
        constexpr uintptr_t prepareRepairAssisteeTime = 0x19c0;
        constexpr uintptr_t prepareRepairCooldownsTime = 0x1b38;
        constexpr uintptr_t repairAssistant = 0xc30;
        constexpr uintptr_t repairAssistee = 0xc00;
        constexpr uintptr_t repairCooldowns = 0x1cd0;
        constexpr uintptr_t rocketFuseDist = 0xa00;
        constexpr uintptr_t scoutCooldown = 0x1548;
        constexpr uintptr_t scoutStartTime = 0x1520;
        constexpr uintptr_t smokeScreenActived = 0x1df8;
        constexpr uintptr_t smokeScreenCount = 0x1dd0;
        constexpr uintptr_t stealthArmyMask = 0x13d8;
        constexpr uintptr_t stealthRadiusSq = 0x13b0;
        constexpr uintptr_t superArtilleryCount = 0x1fc0;
        constexpr uintptr_t supportPlaneCatapultsFuseMask = 0x928;
        constexpr uintptr_t supportPlanesCount = 0x900;
        constexpr uintptr_t timeToNextSmokeScreen = 0x1da8;
        constexpr uintptr_t torpedoDiveDepth = 0xa28;
        constexpr uintptr_t underwaterTime = 0x1f98;
        constexpr uintptr_t unitArmyNo = 0x1350;
        constexpr uintptr_t unitState = 0x12c8;
        constexpr uintptr_t visualReloadProgress = 0x960;
        constexpr uintptr_t vulnerableByUnitUId = 0x1240;
    }
}


#pragma comment(lib, "Psapi.lib")

#define in_range(x,a,b) (x>=a&&x<=b) 
#define get_bits(x) (in_range((x&(~0x20)),'A','F')?((x&(~0x20))-'A'+0xa):(in_range(x,'0','9')?x-'0':0))
#define get_byte(x) (get_bits(x[0])<<4|get_bits(x[1]))

uintptr_t sigscan(const char* scanmodule, const char* pattern, int jump)
{
    uintptr_t moduleAdress = 0;
    const auto mod = scanmodule;
    //moduleAdress = LI_MODULE(mod).get<uintptr_t>();

    static auto patternToByte = [](const char* pattern)
        {
            auto       bytes = std::vector<int>{};
            const auto start = const_cast<char*>(pattern);
            const auto end = const_cast<char*>(pattern) + strlen(pattern);

            for (auto current = start; current < end; ++current)
            {
                if (*current == '?')
                {
                    ++current;
                    if (*current == '?')
                        ++current;
                    bytes.push_back(-1);
                }
                else { bytes.push_back(strtoul(current, &current, 16)); }
            }
            return bytes;
        };

    const auto dosHeader = (PIMAGE_DOS_HEADER)moduleAdress;
    if (!dosHeader)
        return NULL;

    const auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)moduleAdress + dosHeader->e_lfanew);

    const auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    auto       patternBytes = patternToByte(pattern);
    const auto scanBytes = reinterpret_cast<std::uint8_t*>(moduleAdress);

    const auto s = patternBytes.size();
    const auto d = patternBytes.data();

    for (auto i = 0ul; i < sizeOfImage - s; ++i)
    {
        bool found = true;
        for (auto j = 0ul; j < s; ++j)
        {
            if (scanBytes[i + j] != d[j] && d[j] != -1)
            {
                found = false;
                break;
            }
        }
        if (found)
        {
            if (!jump)
                return reinterpret_cast<uintptr_t>(&scanBytes[i]);
            else
            {
                uintptr_t patterScanResult = (reinterpret_cast<uintptr_t>(&scanBytes[i]) + jump);
                int relative = *reinterpret_cast<int*>(patterScanResult + 3);
                return (patterScanResult + 7 + relative);
            }
        }
    }
    return NULL;
}

uintptr_t find(uintptr_t range_start, uintptr_t range_end, const char* pattern) {
    const char* pattern_bytes = pattern;

    uintptr_t first_match = 0;

    for (uintptr_t cur_byte = range_start; cur_byte < range_end; cur_byte++) {
        if (!*pattern_bytes)
            return first_match;

        if (*(uint8_t*)pattern_bytes == '\?' || *(uint8_t*)cur_byte == static_cast<uint8_t>(get_byte(pattern_bytes))) {
            if (!first_match)
                first_match = cur_byte;

            if (!pattern_bytes[2])
                return first_match;

            if (*(uint16_t*)pattern_bytes == '\?\?' || *(uint8_t*)pattern_bytes != '\?')
                pattern_bytes += 3;
            else
                pattern_bytes += 2;
        }
        else {
            pattern_bytes = pattern;
            first_match = 0;
        }
    }

    return 0;
}

uintptr_t find(const char* pattern) {
    const char* pattern_bytes = pattern;

    HMODULE hModule = GetModuleHandle(NULL);
    uintptr_t range_start = 0;
    uintptr_t range_size = 0;

    if (hModule != NULL) {
        // Define a MODULEINFO structure to receive the module information
        MODULEINFO moduleInfo;
        if (GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo))) {
            range_start = (uintptr_t)moduleInfo.lpBaseOfDll;
            range_size = moduleInfo.SizeOfImage;
            //std::cout << "Base Address: " << moduleInfo.lpBaseOfDll << std::endl;
            //std::cout << "Size of Module: " << moduleInfo.SizeOfImage << " bytes" << std::endl;
        }
        else {
            std::cout << "Failed to get module information!" << std::endl;
        }
    }
    else {
        std::cout << "Failed to get module handle!" << std::endl;
    }

    //////std::cout << "Range Size: " << range_size << std::endl;
    uintptr_t range_end = range_start + range_size;

    return find(range_start, range_end, pattern);
}

uintptr_t find_rel(const char* pattern, ptrdiff_t position, ptrdiff_t jmp_size, ptrdiff_t instruction_size) {
    auto result = find(pattern);

    if (!result)
    {
        //////std::cout << "[FindRel] !result!" << std::endl;
        return 0;
    }

    result += position;

    auto rel_addr = *reinterpret_cast<int32_t*>(result + jmp_size);
    auto abs_addr = result + instruction_size + rel_addr;

    return abs_addr;
}

uintptr_t find_rel(const char* pattern, ptrdiff_t position = 0, ptrdiff_t jmp_size = 3, ptrdiff_t instruction_size = 7);

void start_console()
{
    AllocConsole();
    FILE* f = 0;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);
}

int real_main(HMODULE hModule)
{
    start_console();
    auto game_base = (uintptr_t)GetModuleHandle(NULL);
    std::cout << "Game Base: " << std::hex << game_base << std::dec << std::endl;
    auto cgame = *reinterpret_cast<uintptr_t*>(game_base + 0x51A82B0);
    std::cout << "cGame: " << std::hex << cgame << std::dec << std::endl;
    auto cgame_sig = find_rel("48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? F3 0F 10 8D F8 06 00 00");
    std::cout << "#define cGameOffset 0x" << std::hex << cgame_sig - game_base << std::dec << std::endl;
    auto cgame_ptr = *reinterpret_cast<uintptr_t*>(cgame_sig);
    std::cout << "cgame_ptr: " << std::hex << cgame_ptr << std::dec << std::endl;

    auto local_player_sig = find_rel("48 8B 0D ?? ?? ?? ?? B0 FF 48 85 C9 74 04 0F B6 41 08 88 44 24 42 48 8D 05 ?? ?? ?? ?? 48 89 44 24 28 48 8D 4C 24 28 B2 FF E8 ?? ?? ?? ?? 48 89 7C 24 28 83 7C 24 48 00 79 10");
    std::cout << "#define localplayerOffset 0x" << std::hex << local_player_sig - game_base << std::dec << std::endl;
    auto local_player_ptr = *reinterpret_cast<uintptr_t*>(local_player_sig);
    std::cout << "local_player_ptr: " << std::hex << local_player_ptr << std::dec << std::endl;


    auto hud_sig = find_rel("48 8B 15 ?? ?? ?? ?? 0F 57 C0 0F 2E 82 94 03 00 00");
    std::cout << "#define hudOffset 0x" << std::hex << hud_sig - game_base << std::dec << std::endl;


    auto yaw_sig = find_rel("48 8B 0D ?? ?? ?? ?? 48 89 88 1C 22 00 00");
    std::cout << "#define yawOffset 0x" << std::hex << yaw_sig - game_base << std::dec << std::endl;

    //48 8D 0D ? ? ? ? 4C 8D 44 24 ? 89 FA 41 B9 ? ? ? ? E8 ? ? ? ? 48 85 C0 74 50
    auto alllistdata_sig = find_rel("48 8D 0D ? ? ? ? 4C 8D 44 24 ? 89 FA 41 B9 ? ? ? ? E8 ? ? ? ? 48 85 C0 74 50");
    std::cout << "#define allListData 0x" << std::hex << alllistdata_sig - game_base << std::dec << std::endl;

    uintptr_t camera_offset = NULL;
    uintptr_t ballistics_offset = NULL;
    uintptr_t telecontrol_offset = NULL;
    uintptr_t gameui_offset = NULL;
    uintptr_t mousepos_offset = NULL;
    uintptr_t localunit_offset = NULL;

    offset_c offset_dumper("offsets");
    offset_c cgame_dump("cgame_offsets");
    offset_c camera_dump("camera_offsets");
    offset_c ballistics_dump("ballistic_offsets");
    offset_c telecontrol_dump("telecontrol_offsets");
    offset_c gameui_dump("gameui_offsets");

    offset_c localplayer_dump("localplayer_offsets");

    offset_c unit_dump("unit_offsets");
    offset_c airmovement_dump("airmovement_offsets");

    if (IsValidPointer((void*)cgame_ptr))
    {
        ballistics_offset = find_ballistics_ptr(cgame_ptr);
        cgame_dump.add("ballistics_offset", ballistics_offset);

        //camera_offset = find_camera_ptr(cgame_ptr);
        //cgame_dump.add("camera_offset", camera_offset);

        auto ballistics = *reinterpret_cast<uintptr_t*>(cgame_ptr + ballistics_offset);
        if (ballistics_offset != NULL)
        {
            auto round_velocity_offset = find_roundvelocity_ptr(ballistics);
            ballistics_dump.add("round_velocity_offset", round_velocity_offset);

            auto round_mass_offset = find_roundmass_ptr(ballistics);
            ballistics_dump.add("round_mass_offset", round_mass_offset);

            auto round_caliber_offset = find_roundcaliber_ptr(ballistics);
            ballistics_dump.add("round_caliber_offset", round_caliber_offset);

            auto round_length_offset = find_roundlength_ptr(ballistics);
            ballistics_dump.add("round_length_offset", round_length_offset);

            telecontrol_offset = find_telecontrol_ptr(ballistics);
            ballistics_dump.add("telecontrol_offset", telecontrol_offset);
            if (telecontrol_offset != 0)
            {
                auto telecontrol = *reinterpret_cast<uintptr_t*>(ballistics + telecontrol_offset);
                gameui_offset = find_gameui_ptr(telecontrol);
                telecontrol_dump.add("gameui_offset", gameui_offset);
                if (gameui_offset != 0)
                {
                    auto gameui = *reinterpret_cast<uintptr_t*>(telecontrol + gameui_offset);
                    mousepos_offset = find_mouseposition_ptr(gameui);
                    gameui_dump.add("mousepos_offset", mousepos_offset);
                }
            }
        }
    }

    if (IsValidPointer((void*)local_player_ptr))
    {
        localunit_offset = find_localunit_ptr(local_player_ptr);
        localplayer_dump.add("localunit_offset", localunit_offset);
        auto localplayer_offsets = find_localplayer_offsets(local_player_ptr);
        for (auto offset : localplayer_offsets)
        {
            localplayer_dump.add(offset.name, offset.offset);
        }
    }

    if (localunit_offset != NULL)
    {
        auto local_unit = *reinterpret_cast<uintptr_t*>(local_player_ptr + localunit_offset);
        if (IsValidPointer((void*)local_unit))
        {
            auto unit_offset = find_unit_offsets(local_unit);
            for (auto offset : unit_offset)
            {
                unit_dump.add(offset.name, offset.offset);
            }

            auto bbmin_offset = find_bbmin_ptr(local_unit);
            unit_dump.add("bbmin_offset", bbmin_offset);

            auto bbmax_offset = find_bbmax_ptr(local_unit);
            unit_dump.add("bbmax_offset", bbmax_offset);

            auto position_offset = find_position_ptr(local_unit);
            unit_dump.add("position_offset", position_offset);
            unit_dump.add("airmovement_offset", position_offset + 0x10);

            auto airmovement = *reinterpret_cast<uintptr_t*>(local_unit + position_offset + 0x10);

            if (IsValidPointer((void*)airmovement))
            {
                auto velocity_offset = airmovement::find_velocity_ptr(airmovement);
                airmovement_dump.add("velocity_offset", velocity_offset);
            }

            auto groundmovement_offset = find_groundmovement_ptr(local_unit);
            unit_dump.add("groundmovement_offset", groundmovement_offset);
        }
    }
    //Setup CGame path.
    telecontrol_dump.add_child(gameui_dump);
    ballistics_dump.add_child(telecontrol_dump);
    cgame_dump.add_child(ballistics_dump);

    //Add CGame to dump.
    offset_dumper.add_child(cgame_dump);

    //Add localplayer to dump.
    offset_dumper.add_child(localplayer_dump);

    //Add AirMovement to unit.
    unit_dump.add_child(airmovement_dump);

    //Add Unit to dump.
    offset_dumper.add_child(unit_dump);

    //Add normal addresses to dump.
    offset_dumper.add("cgame_offset", cgame_sig - game_base);    
    offset_dumper.add("localplayer_offset", local_player_sig - game_base);
    offset_dumper.add("hud_offset", hud_sig - game_base);
    offset_dumper.add("yaw_offset", yaw_sig - game_base);
    offset_dumper.add("alllistdata_offset", alllistdata_sig - game_base);
    offset_dumper.dump();
    //std::cout << "}" << std::endl;


    //std::cout << "Done!" << std::endl;

    while (!GetAsyncKeyState(VK_END))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    std::cout << "Unloaded!" << std::endl;
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)real_main, hModule, 0, 0);
	}
	return TRUE;
}