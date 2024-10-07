#include <Windows.h>
#include <inttypes.h>
#include <iostream>
#include <map>
#include <unordered_map>
#include <string>
#include <string>

class offset_c
{
private:
    std::string name_space;
    std::map<std::string, uintptr_t> offsets;
    std::vector<offset_c> children;

public:
    offset_c(std::string namespz)
    {
        name_space = namespz;
    };

    void add(std::string name, uintptr_t offset)
    {
        offsets.emplace(name, offset);
    };

    void add_child(offset_c child)
    {
        children.emplace_back(child);
    }

    void dump(int offset = 0)
    {
        std::string spacer = "";
        for (int i = 0; i < offset; i++)
        {
            spacer += " ";
        }
        std::cout << spacer << "namespace " << name_space << std::endl;
        std::cout << spacer << "{" << std::endl;
        auto offset_space = spacer;
        offset_space += " ";
        for (auto& offset : offsets)
        {
            std::cout << offset_space << "constexpr uintptr_t " << offset.first << " = 0x" << std::hex << offset.second << std::dec << "; " << std::endl;
        }
        for (auto& child : children)
        {
            child.dump(offset + 1);
        }
        std::cout << spacer << "}" << std::endl;
    }
};

class cBallistics
{
public:
    int64_t N0000027F; //0x0000
}; //Size: 0x0208

class Vector3
{
public:
    float x, y, z;
};

bool IsValidPointer(void* ptr)
{
    bool valid = ptr != nullptr && !IsBadReadPtr(ptr, sizeof(void*));
    if (!valid)
        return false;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)))
    {
        DWORD protect = mbi.Protect;
        if (protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))
        {
            return true;
        }
    }

    return false;
}

bool are_floats_equal(float a, float b, float epsilon = 0.001f)
{
    return std::fabs(a - b) < epsilon;
}

namespace cgame
{
    uintptr_t find_camera_ptr(uintptr_t cgame)
    {
        for (uintptr_t offset = 0x0; offset < 0x1000; offset += 0x8)
        {
            uintptr_t addr = *reinterpret_cast<uintptr_t*>(cgame + offset);
            //std::cout << "Getting pointer at " << std::hex << offset << std::dec << std::endl;
            if (IsValidPointer((void*)addr))
            {
                for (uintptr_t offset2 = 0x0; offset2 < 0x1000; offset2 += 0x8)
                {
                    //std::cout << "Getting pointer2 at " << std::hex << offset2 << std::dec << std::endl;
                    auto name_ptr = *reinterpret_cast<void**>(addr + offset2);
                    //std::cout << "Name Pointer: " << std::hex << name_ptr << std::endl;
                     // Check if the pointer is valid before dereferencing
                    if (IsValidPointer(name_ptr)) {
                        //std::cout << "Getting string!" << std::endl;
                        char buf[0x50] = { 0 }; // Ensure the buffer is initialized
                        // Copy data from the valid pointer to buf
                        if (ReadProcessMemory(GetCurrentProcess(), name_ptr, buf, sizeof(buf) - 1, nullptr)) {
                            buf[sizeof(buf) - 1] = '\0'; // Null-terminate the string to prevent overflow
                            std::string found_text = std::string(buf);

                            //std::cout << "Found Text: " << found_text << std::endl;
                            if (!found_text.empty() && found_text.find("lense_color") != std::string::npos) {
                                return offset;
                            }
                        }
                    }
                }
            }
        }
        return NULL;
    }

    uintptr_t find_ballistics_ptr(uintptr_t cgame)
    {
        for (uintptr_t offset = 0x0; offset < 0x1000; offset += 0x8)
        {
            __try
            {
                //std::cout << "Searching offset: " << std::hex << offset << std::dec << std::endl;
                auto pointer = *reinterpret_cast<cBallistics**>(cgame + offset);
                if (IsValidPointer(pointer))
                {
                    //std::cout << "Pointer: " << std::hex << pointer << std::dec << std::endl;
                    if ((uintptr_t)pointer > 0x1000000 || (uintptr_t)pointer != 0x10101010000)
                    {
                        int64_t val1 = pointer->N0000027F;
                        //std::cout << "Val1: " << val1 << std::endl;
                        if (val1 == -4294967296) //
                        {
                            return offset;
                        }
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                std::cout << "Exception caught at offset: " << std::hex << offset << std::dec << std::endl;
            }
        }

        return NULL;
    }

    namespace ballistics
    {
        uintptr_t find_roundvelocity_ptr(uintptr_t ballistics)
        {
            for (uintptr_t offset = 0x1000; offset < 0x2000; offset += 0x4)
            {
                //std::cout << "Getting pointer at " << std::hex << offset << std::dec << std::endl;
                auto value = *reinterpret_cast<float*>(ballistics + offset);
                //std::cout << "X: " << mouse_x << std::endl;
                if ((int)value == 1575)
                {
                    return offset;
                }
            }
        }

        uintptr_t find_roundmass_ptr(uintptr_t ballistics)
        {
            for (uintptr_t offset = 0x1000; offset < 0x2000; offset += 0x4)
            {
                //std::cout << "Getting pointer at " << std::hex << offset << std::dec << std::endl;
                auto value = *reinterpret_cast<float*>(ballistics + offset);
                if (value > 4.88f && value < 4.89) //4.88746
                {
                    return offset;
                }
            }

            return NULL;
        }

        uintptr_t find_roundcaliber_ptr(uintptr_t ballistics)
        {
            for (uintptr_t offset = 0x1000; offset < 0x2000; offset += 0x4)
            {
                //std::cout << "Getting pointer at " << std::hex << offset << std::dec << std::endl;
                auto value = *reinterpret_cast<float*>(ballistics + offset);

                if (value == 0.022f)
                {

                    return offset;
                }
            }
        }

        uintptr_t find_roundlength_ptr(uintptr_t ballistics)
        {
            for (uintptr_t offset = 0x1000; offset < 0x2000; offset += 0x4)
            {
                //std::cout << "Getting pointer at " << std::hex << offset << std::dec << std::endl;
                auto value = *reinterpret_cast<float*>(ballistics + offset);
                if (value > 0.9f && value < 0.95)
                {
                    return offset;
                }
            }
        }

        uintptr_t find_telecontrol_ptr(uintptr_t ballistics)
        {
            for (uintptr_t offset = 0x0; offset < 0x1000; offset += 0x8)
            {
                //std::cout << "Getting pointer at " << std::hex << offset << std::dec << std::endl;
                auto name_ptr = *reinterpret_cast<void**>(ballistics + offset);
                // std::cout << "Name Pointer: " << std::hex << name_ptr << std::endl;
                 // Check if the pointer is valid before dereferencing
                if (IsValidPointer(name_ptr)) {
                    //std::cout << "Getting string!" << std::endl;
                    char buf[0x50] = { 0 }; // Ensure the buffer is initialized
                    // Copy data from the valid pointer to buf
                    if (ReadProcessMemory(GetCurrentProcess(), name_ptr, buf, sizeof(buf) - 1, nullptr)) {
                        buf[sizeof(buf) - 1] = '\0'; // Null-terminate the string to prevent overflow
                        std::string found_text = std::string(buf);

                        //std::cout << "Found Text: " << found_text << std::endl;
                        if (!found_text.empty() && found_text.find("telecontrol") != std::string::npos) {
                            return offset - (2 * 0x8);
                        }
                    }
                }
            }
            return NULL;
        }

        namespace telecontrol
        {
            uintptr_t find_gameui_ptr(uintptr_t telecontrol)
            {
                for (uintptr_t offset = 0x0; offset < 0x1000; offset += 0x8)
                {
                    //std::cout << "Getting pointer at " << std::hex << offset << std::dec << std::endl;
                    auto found_ptr = *reinterpret_cast<void**>(telecontrol + offset);
                    // std::cout << "Name Pointer: " << std::hex << name_ptr << std::endl;
                     // Check if the pointer is valid before dereferencing
                    if (IsValidPointer(found_ptr))
                    {
                        auto name_ptr = *reinterpret_cast<void**>(found_ptr);
                        if (IsValidPointer(name_ptr))
                        {
                            //std::cout << "Getting string!" << std::endl;
                            char buf[0x50] = { 0 }; // Ensure the buffer is initialized
                            // Copy data from the valid pointer to buf
                            if (ReadProcessMemory(GetCurrentProcess(), name_ptr, buf, sizeof(buf) - 1, nullptr)) {
                                buf[sizeof(buf) - 1] = '\0'; // Null-terminate the string to prevent overflow
                                std::string found_text = std::string(buf);

                                //std::cout << "Found Text: " << found_text << std::endl;
                                if (!found_text.empty() && found_text.find("ui/gameuiskin") != std::string::npos) {
                                    return offset + (2 * 0x8);
                                }
                            }
                        }
                    }
                }
                return NULL;
            }

            namespace gameui
            {
                uintptr_t find_mouseposition_ptr(uintptr_t gameui)
                {
                    for (uintptr_t offset = 0x0; offset < 0x1000; offset += 0x8)
                    {
                        int monitor_width = 0;
                        HMONITOR hMonitor = MonitorFromWindow(GetConsoleWindow(), MONITOR_DEFAULTTONEAREST);
                        MONITORINFO mi;
                        mi.cbSize = sizeof(mi);

                        if (GetMonitorInfo(hMonitor, &mi)) {
                            monitor_width = mi.rcMonitor.right - mi.rcMonitor.left;
                            //std::cout << "Monitor Width: " << monitor_width << std::endl;
                            int height = mi.rcMonitor.bottom - mi.rcMonitor.top;
                        }
                        //std::cout << "Getting pointer at " << std::hex << offset << std::dec << std::endl;
                        auto mouse_x = *reinterpret_cast<float*>(gameui + offset);
                        //std::cout << "X: " << mouse_x << std::endl;
                        if ((int)mouse_x == (monitor_width / 2))
                        {
                            return offset;
                        }
                    }
                    return NULL;
                }
            }
        }
    }
}

namespace localplayer
{
    uintptr_t find_localunit_ptr(uintptr_t localplayer)
    {
        for (uintptr_t offset = 0x0; offset < 0x1000; offset += 0x1)
        {
            //std::cout << "Getting pointer at " << std::hex << offset << std::dec << std::endl;
            auto name_ptr = *reinterpret_cast<void**>(localplayer + offset);
            // std::cout << "Name Pointer: " << std::hex << name_ptr << std::endl;
             // Check if the pointer is valid before dereferencing
            if (IsValidPointer(name_ptr)) {
                //std::cout << "Getting string!" << std::endl;
                char buf[0x50] = { 0 }; // Ensure the buffer is initialized
                // Copy data from the valid pointer to buf
                if (ReadProcessMemory(GetCurrentProcess(), name_ptr, buf, sizeof(buf) - 1, nullptr)) {
                    buf[sizeof(buf) - 1] = '\0'; // Null-terminate the string to prevent overflow
                    std::string found_text = std::string(buf);

                    //std::cout << "Found Text: " << found_text << std::endl;
                    if (!found_text.empty() && found_text.find("ownedUnitRef") != std::string::npos) {
                        return offset + (4 * 0x8);
                    }
                }
            }
        }
        return NULL;
    }
}

namespace unit
{
    struct unit_offset_info
    {
        std::string name;
        uintptr_t offset;
    };

    std::vector<unit_offset_info> find_unit_offsets(uintptr_t localunit)
    {
        std::unordered_map<std::string, unit_offset_info> found_offset;
        std::vector<unit_offset_info> offset_info;
        for (uintptr_t offset = 0x0; offset < 0x4000; offset += 0x1)
        {
            auto name_ptr = *reinterpret_cast<void**>(localunit + offset);
            if (IsValidPointer(name_ptr))
            {
                char buf[0x50] = { 0 };
                if (ReadProcessMemory(GetCurrentProcess(), name_ptr, buf, sizeof(buf) - 1, nullptr)) {
                    buf[sizeof(buf) - 1] = '\0';
                    std::string found_text = std::string(buf);

                    if (!found_text.empty() && found_text.size() > 5)
                    {
                        bool invalid_string = false;
                        for (int i = 0; i < found_text.size(); i++)
                        {
                            if (!isalpha(found_text.at(i)))
                            {
                                invalid_string = true;
                                break;
                            }
                        }

                        if (invalid_string)
                            continue;

                        if (found_offset.find(found_text) != found_offset.end())
                        {
                            std::cout << "Duplicate: " << found_text << " | " << std::hex << offset + 0x18 << std::dec << std::endl;
                            continue;
                        }

                        unit_offset_info info;
                        info.name = found_text;
                        info.offset = offset + 0x18;
                        found_offset.emplace(found_text, info);
                        offset_info.emplace_back(info);
                    }
                }
            }
        }
        return offset_info;
    }

    uintptr_t find_bbmin_ptr(uintptr_t localunit)
    {
        for (uintptr_t offset = 0x0; offset < 0x2000; offset += 0x1)
        {
            auto value = *reinterpret_cast<Vector3*>(localunit + offset);
            if (are_floats_equal(value.x, -4.731f) && are_floats_equal(value.y, -0.002f) && are_floats_equal(value.z, -1.889f))//value > -4.731)
            {
                return offset;
            }
        }

        return NULL;
    }

    uintptr_t find_bbmax_ptr(uintptr_t localunit)
    {
        for (uintptr_t offset = 0x0; offset < 0x2000; offset += 0x1)
        {
            auto value = *reinterpret_cast<Vector3*>(localunit + offset);
            if (are_floats_equal(value.x, 5.451f) && are_floats_equal(value.y, 3.393f) && are_floats_equal(value.z, 1.829f))//value > -4.731)
            {
                return offset;
            }
        }

        return NULL;
    }

    uintptr_t find_groundmovement_ptr(uintptr_t localunit)
    {
        for (uintptr_t offset = 0x0; offset < 0x4000; offset += 0x1)
        {
            //std::cout << "Getting pointer at " << std::hex << offset << std::dec << std::endl;
            auto name_ptr = *reinterpret_cast<void**>(localunit + offset);
            // std::cout << "Name Pointer: " << std::hex << name_ptr << std::endl;
             // Check if the pointer is valid before dereferencing
            if (IsValidPointer(name_ptr))
            {
                //std::cout << "Getting string!" << std::endl;
                char buf[0x50] = { 0 }; // Ensure the buffer is initialized
                // Copy data from the valid pointer to buf
                if (ReadProcessMemory(GetCurrentProcess(), name_ptr, buf, sizeof(buf) - 1, nullptr))
                {
                    buf[sizeof(buf) - 1] = '\0'; // Null-terminate the string to prevent overflow
                    std::string found_text = std::string(buf);

                    //std::cout << "Found Text: " << found_text << std::endl;
                    if (!found_text.empty() && found_text.find("moveSysTraceRI") != std::string::npos)
                    {
                        //std::cout << "moveSysTraceRI found!" << std::endl;
                        for (uintptr_t offset2 = 0x1; offset2 < 0x100; offset2 += 0x1)
                        {
                            auto newoffset = offset - offset2;
                            //std::cout << "Checking offset: " << std::hex << newoffset << std::dec << std::endl;
                            auto value_ptr = *reinterpret_cast<void**>(localunit + newoffset);
                            // std::cout << "Name Pointer: " << std::hex << name_ptr << std::endl;
                             // Check if the pointer is valid before dereferencing
                            if (IsValidPointer(value_ptr))
                            {
                                //std::cout << "Offset " << std::hex << newoffset << std::dec << " is a valid pointer!" << std::endl;
                                for (uintptr_t offset3 = 0x0; offset3 < 0x500; offset3 += 0x1)
                                {
                                    auto value = *reinterpret_cast<float*>((uintptr_t)value_ptr + offset3);
                                    if (are_floats_equal(value, 180.0f))
                                    {
                                        return newoffset;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return NULL;
    }

}