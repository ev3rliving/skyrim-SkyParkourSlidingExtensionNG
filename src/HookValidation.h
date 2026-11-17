#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <limits>

namespace Slide::Validation {
inline bool Accessible(std::uintptr_t address, std::size_t size, bool executable=false) {
    if(!address || !size || address>std::numeric_limits<std::uintptr_t>::max()-size) return false;
    MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(reinterpret_cast<const void*>(address),&info,sizeof(info)) ||
        info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
    const auto end=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
    if(address+size>end) return false;
    const auto protection=info.Protect&0xFF;
    if(executable) return protection==PAGE_EXECUTE || protection==PAGE_EXECUTE_READ ||
        protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY;
    return protection==PAGE_READONLY || protection==PAGE_READWRITE || protection==PAGE_WRITECOPY ||
        protection==PAGE_EXECUTE_READ || protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY;
}
inline bool HookTarget(std::uintptr_t table,std::size_t slot) {
    if(slot>(std::numeric_limits<std::uintptr_t>::max()-table)/sizeof(std::uintptr_t)) return false;
    const auto address=table+slot*sizeof(std::uintptr_t);
    if(!Accessible(address,sizeof(std::uintptr_t))) return false;
    std::uintptr_t function{};
    std::memcpy(&function,reinterpret_cast<const void*>(address),sizeof(function));
    return Accessible(function,1,true);
}
}
