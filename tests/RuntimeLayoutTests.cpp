#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <iostream>
int main() {
    alignas(RE::PlayerCharacter) std::byte actorStorage[sizeof(RE::PlayerCharacter)+1024]{};
    auto* actor=reinterpret_cast<RE::PlayerCharacter*>(actorStorage);
    alignas(RE::BSAnimationGraphManager) std::byte graphStorage[sizeof(RE::BSAnimationGraphManager)+64]{};
    auto* manager=reinterpret_cast<RE::BSAnimationGraphManager*>(graphStorage);
    for(const auto v:{REL::Version{1,5,97,0},REL::Version{1,6,1170,0},REL::Version{1,6,1179,0},REL::Version{1,7,99,0},REL::Version{1,7,104,0}}) {
        if(!REL::Module::mock(v)) return 1;
        const auto ae=v>=REL::Version{1,6,0,0};
        const auto offset=reinterpret_cast<std::uintptr_t>(actor->AsActorState())-reinterpret_cast<std::uintptr_t>(actor);
        if(REL::Module::IsAE()!=ae || REL::RelocationID(11,22).id()!=(ae?22:11) || offset!=(ae?0xC0:0xB8)) return 2;
        const auto graphOffset=reinterpret_cast<std::uintptr_t>(&manager->GetRuntimeData())-reinterpret_cast<std::uintptr_t>(manager);
        if(graphOffset!=(ae?0xA0:0x98)) return 3;
        std::cout<<v.string()<<" actor-state accessor and SE/AE selection passed\n";
    }
    REL::Module::reset();
}
