#include "RuntimeSupport.h"
#include "HookValidation.h"
#include <iostream>
int main() {
    using namespace Slide;
    for(auto v:{Runtime::Pack(1,5,97),Runtime::Pack(1,6,1170),Runtime::Pack(1,6,1179),Runtime::Pack(1,7,99),Runtime::Pack(1,7,104)})
        if(!Runtime::Supported(v)) return 1;
    for(auto v:{Runtime::Pack(1,4,15),Runtime::Pack(1,7,105),Runtime::Pack(1,6,640),Runtime::Pack(2,0,0)})
        if(Runtime::Supported(v)) return 2;
    if(Validation::HookTarget(0,1)) return 3;
    std::uintptr_t bad=1;
    if(Validation::HookTarget(reinterpret_cast<std::uintptr_t>(&bad),0)) return 4;
    auto* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!page) return 5;
    std::uintptr_t target=reinterpret_cast<std::uintptr_t>(page);
    if(Validation::HookTarget(reinterpret_cast<std::uintptr_t>(&target),0)) return 6;
    DWORD old{};
    if(!VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old)) return 7;
    if(!Validation::HookTarget(reinterpret_cast<std::uintptr_t>(&target),0)) return 8;
    VirtualFree(page,0,MEM_RELEASE);
    std::cout<<"PASS runtime allowlist, unreadable target and non-executable target rejection\n";
}
