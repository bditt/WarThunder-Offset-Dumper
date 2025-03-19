#include <Windows.h>
#include <vector>
#include <iostream>
#include <stdio.h>
#include <inttypes.h>
#include <thread>
#include <psapi.h>
#include <filesystem>

#include "lazy_importer.hpp"
#include "finder_funcs.hpp"

using namespace cgame;
using namespace cgame::camera;
using namespace cgame::ballistics;
using namespace cgame::ballistics::telecontrol;
using namespace cgame::ballistics::telecontrol::gameui;
using namespace localplayer;
using namespace unit;
using namespace unit::turret;
using namespace unit::turret::weapon_information;

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
    SetConsoleTitleA("bditt's War Thunder offset dumper! https://github.com/bditt/WarThunder-Offset-Dumper");

    auto game_base = (uintptr_t)GetModuleHandle(NULL);
    auto cgame_sig = find_rel("48 8B 05 ?? ?? ?? ?? 48 85 C0 74 0F 0F B6 80 D3 00 00 00");
    auto local_player_sig = find_rel("48 8B 0D ?? ?? ?? ?? 48 85 C9 74 0B B2 01"); //"48 8B 0D ?? ?? ?? ?? B0 FF 48 85 C9 74 04 0F B6 41 08 88 44 24 42 48 8D 05 ?? ?? ?? ?? 48 89 44 24 28 48 8D 4C 24 28 B2 FF E8 ?? ?? ?? ?? 48 89 7C 24 28 83 7C 24 48 00 79 10");
    auto hud_sig = find_rel("48 8B 0D ?? ?? ?? ?? 31 D2 80 79 35 00 0F 94 C2 80 BC 56 9C 00 00 00 00");
    auto yaw_sig = find_rel("48 8B 0D ? ? ? ? 48 89 88 ? ? ? ? F3 0F 10 80");
    auto alllistdata_sig = find_rel("48 8D 0D ?? ?? ?? ?? 4C 8D 44 24 30 E8 ?? ?? ?? ?? 0F 28 74 24 60");

    if (cgame_sig < 0x100000)
    {
        std::cout << "CGame Sig failed!" << std::endl;
        return 0;
    }

    if (local_player_sig < 0x100000)
    {
        std::cout << "LocalPlayer Sig failed!" << std::endl;
        return 0;
    }

    if (hud_sig < 0x100000)
    {
        std::cout << "HUD Sig failed!" << std::endl;
        return 0;
    }

    if (yaw_sig < 0x100000)
    {
        std::cout << "Yaw Sig failed!" << std::endl;
        return 0;
    }

    if (alllistdata_sig < 0x100000)
    {
        std::cout << "AllListData Sig failed!" << std::endl;
        return 0;
    }

    std::cout << "alllistdata_sig: 0x" << std::hex << alllistdata_sig - game_base << std::dec << std::endl;
    std::cout << "cgame_sig: 0x" << std::hex << cgame_sig - game_base << std::dec << std::endl;
    std::cout << "hud_sig: 0x" << std::hex << hud_sig - game_base << std::dec << std::endl;
    std::cout << "local_player_sig: 0x" << std::hex << local_player_sig - game_base << std::dec << std::endl;
    std::cout << "yaw_sig: 0x" << std::hex << yaw_sig - game_base << std::dec << std::endl;


    auto cgame_ptr = *reinterpret_cast<uintptr_t*>(cgame_sig);
    auto local_player_ptr = *reinterpret_cast<uintptr_t*>(local_player_sig);

    // Some debug info for me.
    std::cout << "Game Base: " << std::hex << game_base << std::dec << std::endl;
    std::cout << "cgame_ptr: " << std::hex << cgame_ptr << std::dec << std::endl;
    std::cout << "local_player_ptr: " << std::hex << local_player_ptr << std::dec << std::endl;

    std::cout << "Starting dumper!" << std::endl;
    std::remove("dump.hpp");
    std::ofstream outfile("dump.hpp");
    if (!outfile.is_open())
    {
        std::cerr << "Error opening dump file!" << std::endl;
        return 0;
    }

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
    offset_c turret_dump("turret_offsets");
    offset_c weapon_information_dump("weapon_information_offsets");
    offset_c airmovement_dump("airmovement_offsets");

    if (IsValidPointer((void*)cgame_ptr))
    {
        std::cout << "Dumping CGame!" << std::endl;
        ballistics_offset = find_ballistics_ptr(cgame_ptr);
        cgame_dump.add("ballistics_offset", ballistics_offset);

        camera_offset = find_camera_ptr(cgame_ptr);
        cgame_dump.add("camera_offset", camera_offset);

        if (camera_offset != NULL)
        {
            std::cout << "Dumping Camera!" << std::endl;
            auto camera = *reinterpret_cast<uintptr_t*>(cgame_ptr + camera_offset);
            if (camera > 0x100000)
            {
                auto camera_position_offset = find_camera_position_ptr(camera);
                camera_dump.add("camera_position_offset", camera_position_offset);

                auto camera_matrix_offset = find_camera_matrix_ptr(camera);
                camera_dump.add("camera_matrix_offset", camera_matrix_offset);
            }
            cgame_dump.add_child(camera_dump);
        }

        auto ballistics = *reinterpret_cast<uintptr_t*>(cgame_ptr + ballistics_offset);
        if (ballistics_offset != NULL)
        {
            std::cout << "Dumping Ballistics!" << std::endl;
            auto bomb_prediction_offset = find_bombpred_ptr(ballistics);
            ballistics_dump.add("bomb_prediction_offset", bomb_prediction_offset);

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
                std::cout << "Dumping Telecontrol!" << std::endl;
                auto telecontrol = *reinterpret_cast<uintptr_t*>(ballistics + telecontrol_offset);
                gameui_offset = find_gameui_ptr(telecontrol);
                telecontrol_dump.add("gameui_offset", gameui_offset);
                if (gameui_offset != 0)
                {
                    std::cout << "Dumping GameUI!" << std::endl;
                    auto gameui = *reinterpret_cast<uintptr_t*>(telecontrol + gameui_offset);
                    mousepos_offset = find_mouseposition_ptr(gameui);
                    gameui_dump.add("mousepos_offset", mousepos_offset);
                    telecontrol_dump.add_child(gameui_dump);
                }

                ballistics_dump.add_child(telecontrol_dump);
            }

            cgame_dump.add_child(ballistics_dump);
        }
    }

    if (IsValidPointer((void*)local_player_ptr))
    {
        std::cout << "Dumping LocalPlayer!" << std::endl;
        localunit_offset = find_localunit_ptr(local_player_ptr);
        localplayer_dump.add("localunit_offset", localunit_offset);
        auto localplayer_offsets = find_localplayer_offsets(local_player_ptr);
        for (auto offset : localplayer_offsets)
        {
            localplayer_dump.add(offset.name + "_offset", offset.offset);
        }
    }

    if (localunit_offset != NULL)
    {
        auto local_unit = *reinterpret_cast<uintptr_t*>(local_player_ptr + localunit_offset) - 1;
        if (IsValidPointer((void*)local_unit))
        {
            std::cout << "Dumping Unit!" << std::endl;
            //auto unit_list_offset = find_unit_count_ptr(cgame_ptr, local_unit);
            ////Ignore how terrible this unit list shit is, but it works.
            //std::cout << "Dumping unit_count_offset!" << std::endl;
            //cgame_dump.add("unit_count_offset", unit_list_offset + 0x30 + 0x8);
            //std::cout << "Dumping unit_list_offset!" << std::endl;
            //cgame_dump.add("unit_list_offset", unit_list_offset + 0x30);

            std::cout << "Dumping Unit_offsets!" << std::endl;
            auto unit_offset = find_unit_offsets(local_unit);
            for (auto offset : unit_offset)
            {
                unit_dump.add(offset.name + "_offset", offset.offset);
            }

            std::cout << "Dumping bbmin_offset!" << std::endl;
            auto bbmin_offset = find_bbmin_ptr(local_unit);
            unit_dump.add("bbmin_offset", bbmin_offset);

            std::cout << "Dumping bbmax_offset!" << std::endl;
            auto bbmax_offset = find_bbmax_ptr(local_unit);
            unit_dump.add("bbmax_offset", bbmax_offset);

            std::cout << "Dumping position_offset!" << std::endl;
            auto position_offset = find_position_ptr(local_unit);
            unit_dump.add("position_offset", position_offset);
            std::cout << "Dumping airmovement_offset!" << std::endl;
            unit_dump.add("airmovement_offset", position_offset + 0x10);

            std::cout << "Dumping info_offset!" << std::endl;
            auto info_offset = find_info_ptr(local_unit);
            unit_dump.add("info_offset", info_offset);

            std::cout << "Dumping turret_offset!" << std::endl;
            auto turret_offset = find_turret_ptr(local_unit);
            unit_dump.add("turret_offset", turret_offset);

            if (false) //turret_offset != NULL)
            {
                auto turret = *reinterpret_cast<uintptr_t*>(local_unit + turret_offset);
                std::cout << "Dumping weapon_information_offset!" << std::endl;
                auto weapon_information_offset = find_weapon_information_ptr(turret);
                turret_dump.add("weapon_information_offset", weapon_information_offset);
                if (weapon_information_offset != NULL)
                {
                    auto weapon_information = *reinterpret_cast<uintptr_t*>(turret + weapon_information_offset);
                    std::cout << "Dumping weapon_position_offset!" << std::endl;
                    auto weapon_position_offset = find_weapon_position_ptr(weapon_information);
                    weapon_information_dump.add("weapon_position_offset", weapon_position_offset);
                    turret_dump.add_child(weapon_information_dump);
                }

                unit_dump.add_child(turret_dump);
            }

            std::cout << "Dumping rotation_matrix_offset!" << std::endl;
            auto rotation_matrix_offset = find_rotation_matrix_ptr(local_unit);
            unit_dump.add("rotation_matrix_offset", rotation_matrix_offset);

            std::cout << "Dumping airmovement!" << std::endl;
            auto airmovement_offset = local_unit + position_offset + 0x10;
            auto airmovement = *reinterpret_cast<uintptr_t*>(airmovement_offset);

            unit_dump.add("airmovement_offset", airmovement_offset);

            if (IsValidPointer((void*)airmovement))
            {
                std::cout << "Dumping velocity_offset!" << std::endl;
                auto velocity_offset = airmovement::find_velocity_ptr(airmovement);
                airmovement_dump.add("velocity_offset", velocity_offset);
            }

            std::cout << "Dumping groundmovement_offset!" << std::endl;
            auto groundmovement_offset = find_groundmovement_ptr(local_unit);
            unit_dump.add("groundmovement_offset", groundmovement_offset);
        }
    }

    std::cout << "Setting up dump order!" << std::endl;

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


    std::cout << "Writing Dump!" << std::endl;
    offset_dumper.dump(outfile);
    outfile.close();

    std::cout << "Game dumped!" << std::endl;

    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::cout << "Dump located at: " << std::filesystem::path(path).parent_path().string() << "\\dump.hpp" << std::endl;

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