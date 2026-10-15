#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <Windows.h>
#include <array>
#include <atomic>
#include <string>
#include <mutex>
#include <chrono>
#include "SlidePhysics.h"

namespace {
constexpr REL::Version runtime{1,6,1170,0};
Slide::Config config;
bool enabled=true, debug=true;
std::atomic_bool ready{}, active{}, endRequested{}, clipSeen{}, physicsSeen{};
std::atomic_bool slopeMotion{}, finishing{};
std::atomic_bool recoveryRequested{};
std::atomic<float> clockRate{1}, clipTime{}, stalled{};
std::atomic<RE::hkbCharacter*> primaryGraph{};
Slide::CancelLatch cancelLatch;
std::atomic<RE::bhkCharacterController*> ownedController{};
std::array<std::atomic<RE::hkbCharacter*>,8> graphs{};
float elapsed=0;
float lastRecoveryTime=0;
float filteredGrade=0;
Slide::Vec entryDirection{};
Slide::Grace contactGrace, graphGrace, recoveryStall;
bool stopping=false;
Slide::PendingCleanup cleanup;
struct FloorSample {
    RE::bhkCharacterController* controller{};
    Slide::Vec normal{};
    bool hit{};
    std::chrono::steady_clock::time_point time{};
};
FloorSample floorSample;
std::mutex floorMutex;
bool reportedFlagSupport=false,reportedProbeSupport=false;
static_assert(offsetof(RE::bhkCharacterController,outVelocity)==0x90);
static_assert(offsetof(RE::bhkCharacterController,supportNorm)==0x110);
static_assert(offsetof(RE::bhkCharacterController,surfaceInfo)==0x1A0);
static_assert(offsetof(RE::hkbClipGenerator,localTime)==0xF0);

Slide::Vec Read(const RE::hkVector4& value) {
    alignas(16) float v[4]; _mm_store_ps(v,value.quad); return {v[0],v[1],v[2]};
}
void Write(RE::hkVector4& value, Slide::Vec v) {
    alignas(16) float old[4]; _mm_store_ps(old,value.quad);
    value.quad=_mm_setr_ps(v.x,v.y,v.z,old[3]);
}
bool GraphBool(RE::PlayerCharacter* p,const char* name) {
    bool value=false; return p->GetGraphVariableBool(name,value)&&value;
}
bool Eligible(RE::PlayerCharacter* p) {
    if (!ready || !p || !p->Is3DLoaded()) return false;
    auto* ui=RE::UI::GetSingleton();
    auto* camera=RE::PlayerCamera::GetSingleton();
    if (!ui || ui->GameIsPaused() || !camera || !camera->currentState) return false;
    const auto id=camera->currentState->id;
    if (id!=RE::CameraState::kFirstPerson && id!=RE::CameraState::kThirdPerson &&
        id!=RE::CameraState::kIronSights && id!=RE::CameraState::kPCTransition) return false;
    auto* s=p->AsActorState();
    return s->GetLifeState()==RE::ACTOR_LIFE_STATE::kAlive &&
        s->GetKnockState()==RE::KNOCK_STATE_ENUM::kNormal &&
        s->GetSitSleepState()==RE::SIT_SLEEP_STATE::kNormal &&
        !p->IsOnMount() && !p->IsInKillMove() && !s->IsSwimming() && !s->IsFlying() &&
        !GraphBool(p,"SkyParkourOngoing");
}
bool CanUseGround(RE::bhkCharacterController* c) {
    return c && c->context.currentState==RE::hkpCharacterStateTypes::kOnGround &&
        !c->flags.any(RE::CHARACTER_FLAGS::kNoSim,RE::CHARACTER_FLAGS::kFollowRagdoll);
}
bool NativeSupported(RE::bhkCharacterController* c) {
    if(!c) return false;
    return Slide::HasContact(c->context.currentState==RE::hkpCharacterStateTypes::kOnGround,
        !c->flags.any(RE::CHARACTER_FLAGS::kNoSim,RE::CHARACTER_FLAGS::kFollowRagdoll),
        Slide::Normal(Read(c->supportNorm)),c->flags.any(RE::CHARACTER_FLAGS::kSupport),
        c->surfaceInfo.supportedState!=RE::hkpSurfaceInfo::SupportedState::kUnsupported,false);
}
bool ContactNormal(RE::bhkCharacterController* c,Slide::Vec& normal) {
    if(!CanUseGround(c)) return false;
    normal=Read(c->supportNorm);
    if(Slide::HasContact(true,true,Slide::Normal(normal),
        c->flags.any(RE::CHARACTER_FLAGS::kSupport),
        c->surfaceInfo.supportedState!=RE::hkpSurfaceInfo::SupportedState::kUnsupported,false)) return true;
    std::lock_guard lock(floorMutex);
    const bool fresh=floorSample.controller==c && floorSample.hit &&
        std::chrono::steady_clock::now()-floorSample.time<std::chrono::milliseconds(100);
    if(fresh) normal=floorSample.normal;
    return Slide::HasContact(true,true,Slide::Normal(normal),false,false,fresh);
}
bool Supported(RE::bhkCharacterController* c) {
    Slide::Vec normal; return ContactNormal(c,normal);
}
void SampleFloor(RE::PlayerCharacter* p,RE::bhkCharacterController* c) {
    FloorSample sample{c,{},false,std::chrono::steady_clock::now()};
    if(CanUseGround(c) && !NativeSupported(c)) {
        auto* cell=p->GetParentCell();
        auto* world=cell ? cell->GetbhkWorld() : nullptr;
        if(world) {
            RE::bhkPickData pick{};
            auto pos=p->GetPosition();
            const float scale=RE::bhkWorld::GetWorldScale();
            pick.rayInput.from.quad=_mm_setr_ps(pos.x*scale,pos.y*scale,(pos.z+12.0f)*scale,0);
            pick.rayInput.to.quad=_mm_setr_ps(pos.x*scale,pos.y*scale,(pos.z-24.0f)*scale,0);
            std::uint32_t filter=0; c->GetCollisionFilterInfo(filter);
            // Preserve the player's collision group to exclude its own body.
            pick.rayInput.filterInfo=(filter & ~0x7Fu) | static_cast<std::uint32_t>(RE::COL_LAYER::kCharController);
            pick.rayInput.enableShapeCollectionFilter=true;
            {
                RE::BSReadLockGuard worldLock(world->worldLock);
                if(world->PickObject(pick) && pick.rayOutput.HasHit()) {
                    const auto layer=pick.rayOutput.rootCollidable->GetCollisionLayer();
                    // Probe static floor only; actors, triggers and water cannot
                    // turn an unsupported frame into a sliding surface.
                    const bool floor=layer==RE::COL_LAYER::kStatic || layer==RE::COL_LAYER::kTerrain ||
                        layer==RE::COL_LAYER::kGround || layer==RE::COL_LAYER::kStairHelper;
                    sample.normal=Read(pick.rayOutput.normal);
                    sample.hit=floor && Slide::Normal(sample.normal);
                }
            }
        }
    }
    if(debug && c && NativeSupported(c) && c->surfaceInfo.supportedState==RE::hkpSurfaceInfo::SupportedState::kUnsupported && !reportedFlagSupport) {
        reportedFlagSupport=true; spdlog::info("Using native support flag despite surfaceInfo unsupported.");
    }
    if(debug && sample.hit && !reportedProbeSupport) {reportedProbeSupport=true; spdlog::info("Nearby static floor confirmed by short ground probe.");}
    std::lock_guard lock(floorMutex); floorSample=sample;
}
void PublishGraphs(RE::PlayerCharacter* p) {
    RE::BSAnimationGraphManagerPtr manager;
    p->GetAnimationGraphManager(manager);
    const auto index=manager ? manager->GetRuntimeData().activeGraph : 0;
    primaryGraph=manager && index<manager->graphs.size() && manager->graphs[index] ? &manager->graphs[index]->characterInstance : nullptr;
    for (std::uint32_t i=0;i<graphs.size();++i)
        graphs[i].store(manager && i<manager->graphs.size() && manager->graphs[i] ?
            &manager->graphs[i]->characterInstance : nullptr);
}
void Reset() {
    active=false; endRequested=false; cancelLatch.Start();
    ownedController=nullptr;
    for(auto& g:graphs) g=nullptr;
    elapsed=0;
    slopeMotion=false; finishing=false; clockRate=1; clipTime=0; stalled=0;
    recoveryRequested=false; lastRecoveryTime=0; recoveryStall={};
    reportedFlagSupport=false; reportedProbeSupport=false;
    {std::lock_guard lock(floorMutex);floorSample={};}
    filteredGrade=0; contactGrace={}; graphGrace={}; primaryGraph=nullptr;
}
void BeginRecovery(const char* reason) {
    endRequested=false; recoveryRequested=false;
    if(finishing.exchange(true)) return;
    lastRecoveryTime=clipTime.load(); recoveryStall={}; clockRate=1;
    if(debug) spdlog::info("Recovery requested: {}; clip {:.3f}; letting original recovery/stop annotations play.",reason,clipTime.load());
}
void RetryCleanup(RE::PlayerCharacter* p) {
    // Deliver the same end annotation as the asset. SkyParkour's graph-manager
    // hook releases its yaw/control locks before forwarding the stop transition.
    cleanup.Retry([&] {
        RE::BSAnimationGraphManagerPtr manager;
        if(!p || !p->GetAnimationGraphManager(manager) || !manager ||
            manager->GetRuntimeData().activeGraph>=manager->graphs.size()) return false;
        auto graph=manager->graphs[manager->GetRuntimeData().activeGraph];
        if(!graph) return false;
        {
            RE::BSAnimationGraphEvent event{RE::BSFixedString("SkyParkour_SlideStop"),p,RE::BSFixedString("Slide")};
            RE::BSTEventSource<RE::BSAnimationGraphEvent>* source=graph.get();
            source->SendEvent(&event);
        }
        p->NotifyAnimationGraph("SkyParkour_SlideStop");
        if(debug) spdlog::info("Pending slide cleanup delivered.");
        return true;
    });
}
void Stop(RE::PlayerCharacter* p,const char* reason) {
    if (!active.exchange(false) || stopping) return;
    stopping=true;
    cleanup.Request();
    RetryCleanup(p);
    if(cleanup.Pending() && debug) spdlog::info("Slide cleanup deferred until animation graph returns.");
    if(debug) spdlog::info("Slide ended: {}; elapsed {:.3f}s, clip {:.3f}, rate {:.2f}, missing contact {:.3f}s (animation {}, physics {})",reason,elapsed,clipTime.load(),clockRate.load(),contactGrace.missing,clipSeen.load(),physicsSeen.load());
    Reset(); stopping=false;
}

struct NotifyHook {
    static bool Call(RE::IAnimationGraphManagerHolder* holder,const RE::BSFixedString& event) {
        auto* p=RE::PlayerCharacter::GetSingleton();
        if(!p || holder!=static_cast<RE::IAnimationGraphManagerHolder*>(p)) return original(holder,event);
        if(event=="SkyParkour_Slide" && cleanup.Pending()) return false;
        if(active && (event=="SkyParkour_SlideToSneak" || event=="SkyParkour_SlideToSneak_Adv")) return false;
        if(active && event=="SkyParkour_SlideStop" && !stopping) {
            // External exit: do not delay the native transition or keep a frozen clip.
            if(debug) spdlog::info("Native slide exit; elapsed {:.3f}s, clip {:.3f}, rate {:.2f}",elapsed,clipTime.load(),clockRate.load());
            active=false; ownedController=nullptr;
        }
        if(event!="SkyParkour_Slide" || active || !Eligible(p) ||
            GraphBool(p,"SkyParkourIsLandingRoll") || !p->AsActorState()->IsSprinting()) return original(holder,event);
        auto* c=p->GetCharController();
        SampleFloor(p,c);
        if(!Supported(c)) return original(holder,event);
        RE::hkVector4 measured; c->GetLinearVelocityImpl(measured);
        Slide::Vec n;
        if(!ContactNormal(c,n) || !Slide::UnitNormal(n)) return original(holder,event);
        auto v=Slide::Tangent(Read(measured)-Read(c->surfaceInfo.surfaceVelocity),n);
        if(!v.finite() || v.length()<config.minEntrySpeed) return original(holder,event);
        Reset();
        entryDirection=v*(1/v.length());
        PublishGraphs(p);
        clipSeen=false; physicsSeen=false; endRequested=false; cancelLatch.Start();
        elapsed=0; ownedController=c; active=true;
        const bool accepted=original(holder,event);
        if(!accepted) Reset();
        else if(debug) spdlog::info("Momentum slide started at {:.3f} m/s",v.length());
        return accepted;
    }
    static inline REL::Relocation<decltype(&Call)> original;
};

struct ClipHook {
    static void Call(RE::hkbClipGenerator* clip,const RE::hkbContext& context,float dt) {
        bool ours=false;
        if(active && context.character) for(auto& g:graphs) if(g.load()==context.character) {ours=true; break;}
        const char* name=clip->animationName.c_str();
        if(ours && name && std::string_view(name).find("SkyParkour_Slide")!=std::string_view::npos &&
            std::string_view(name).find("SprintSlide.hkx")!=std::string_view::npos) {
            if(!clipSeen.exchange(true) && debug) spdlog::info("Slide animation intercepted: {}",name);
            // Rate 1 passes the exact original dt; flat slides therefore retain
            // stock timing and root motion, including configured playback speed.
            const float speed=clip->playbackSpeed;
            if(std::isfinite(speed) && speed>0) {
                const float rate=Slide::PlaybackRate(clockRate.load(),finishing.load());
                if(rate!=1) dt=Slide::Advance(clip->localTime,dt*speed,rate)/speed;
            }
        }
        original(clip,context,dt);
        if(ours && name && std::string_view(name).find("SprintSlide.hkx")!=std::string_view::npos && context.character==primaryGraph.load())
            clipTime=clip->localTime;
    }
    static inline REL::Relocation<decltype(&Call)> original;
};

struct GroundHook {
    static void Call(RE::bhkCharacterStateOnGround* state,RE::bhkCharacterController* c) {
        RE::hkVector4 actual{};
        Slide::Vec normal{},surface{};
        const bool ours=active && c==ownedController.load() && ContactNormal(c,normal) && Slide::UnitNormal(normal);
        float dt=0;
        if(ours) {
            c->GetLinearVelocityImpl(actual);
            surface=Read(c->surfaceInfo.surfaceVelocity);
            dt=c->stepInfo.deltaTime;
        }
        original(state,c);
        // Chain native gravity/transitions and other plugins. Replace only the
        // slide's tangential output before controller collision integration.
        if(!ours || !active || !Supported(c)) return;
        if(!physicsSeen.exchange(true) && debug) spdlog::info("Momentum ground physics active.");
        const float weight=slopeMotion ? Slide::MotionWeight(clipTime.load()) : 0;
        if(weight<=0 || !surface.finite() || !std::isfinite(dt) || dt<=0 || dt>0.1f) return;
        const auto measured=Read(actual)-surface;
        if(!measured.finite()) return;
        auto result=Slide::Step(measured,normal,dt,config);
        // A contact/velocity fluctuation no longer ends a slide in one frame.
        // A sustained obstruction releases the held pose into its normal exit.
        const float low=measured.length()<0.3f ? stalled.load()+dt : 0.0f;
        stalled=low;
        if(low>0.25f && !finishing) recoveryRequested=true;
        const auto native=Read(c->outVelocity);
        if(!native.finite()) return;
        // Retain native normal velocity, including gravity/contact correction.
        const auto base=Slide::Tangent(native-surface,normal);
        const auto output=base*(1-weight)+result*weight+surface+normal*((native-surface).dot(normal));
        if(output.finite()) Write(c->outVelocity,output);
    }
    static inline REL::Relocation<decltype(&Call)> original;
};

struct PlayerHook {
    static void Call(RE::PlayerCharacter* p,float dt) {
        if(ready && cleanup.Pending()) RetryCleanup(p);
        if(active) {
            auto* ui=RE::UI::GetSingleton();
            if(ui && !ui->GameIsPaused()) {
                if(std::isfinite(dt) && dt>0) elapsed+=dt;
                auto* c=p->GetCharController();
                if(endRequested) BeginRecovery("second crouch press");
                if(recoveryRequested) BeginRecovery("sustained stall");
                SampleFloor(p,c);
                const float currentClip=clipTime.load();
                const bool recoveryStuck=finishing && recoveryStall.Update(currentClip>lastRecoveryTime+0.00001f,dt,1.0f);
                lastRecoveryTime=currentClip;
                if(!Eligible(p)) Stop(p,"actor/camera state interrupted slide");
                else if(c!=ownedController.load() || !c) Stop(p,"character controller replaced");
                else if(recoveryStuck) Stop(p,"recovery animation failed to advance for one second");
                else if(!finishing && contactGrace.Update(Supported(c),dt,config.contactGrace)) {
                    if(debug) spdlog::info("Contact timeout: state {}, support {}, flags {}, normal.z {:.3f}",
                        static_cast<int>(c->context.currentState),static_cast<int>(c->surfaceInfo.supportedState.get()),
                        static_cast<unsigned>(c->flags.get()),Read(c->supportNorm).z);
                    BeginRecovery("ground contact absent beyond grace interval");
                } else if(elapsed>1.5f && !clipSeen) Stop(p,"slide animation hook not observed");
                else if(elapsed>0.25f && graphGrace.Update(GraphBool(p,"SkyParkourSliding"),dt,0.2f))
                    Stop(p,"slide graph absent beyond grace interval");
                else {
                    PublishGraphs(p);
                    Slide::Vec n;
                    if(ContactNormal(c,n) && Slide::UnitNormal(n)) {
                        RE::hkVector4 measured; c->GetLinearVelocityImpl(measured);
                        auto v=Slide::Tangent(Read(measured)-Read(c->surfaceInfo.surfaceVelocity),n);
                        // Use direction along the actual slope, not camera pitch.
                        auto direction=v.finite() && v.length()>0.3f ? v*(1/v.length()) : Slide::Tangent(entryDirection,n);
                        const float len=direction.length();
                        const float grade=len>0.01f ? std::asin(std::clamp(-direction.z/len,-1.0f,1.0f))*57.2957795f : 0;
                        const float blend=std::isfinite(dt)&&dt>0 ? 1-std::exp(-dt/0.06f) : 0;
                        filteredGrade+=(grade-filteredGrade)*blend;
                        const float rate=Slide::Rate(filteredGrade,config);
                        clockRate=rate;
                        if(rate!=1 && clipTime<Slide::middleEnd) slopeMotion=true;
                    }
                    // Missing contact freezes only the terrain estimate. Native
                    // airborne physics remains in control until support returns.
                }
            }
        }
        original(p,dt);
    }
    static inline REL::Relocation<decltype(&Call)> original;
};
class InputSink final: public RE::BSTEventSink<RE::InputEvent*> {
public:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
        if(!events || !active) return RE::BSEventNotifyControl::kContinue;
        auto* ui=RE::UI::GetSingleton();
        auto* user=RE::UserEvents::GetSingleton();
        if(!user || !ui || ui->GameIsPaused()) return RE::BSEventNotifyControl::kContinue;
        for(auto* e=*events;e;e=e->next) if(auto* b=e->AsButtonEvent(); b && e->QUserEvent()==user->sneak) {
            if(cancelLatch.Input(b->IsDown(),b->IsUp())) endRequested=true;
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
void ReadSettings() {
    const auto path=std::filesystem::absolute("Data/SKSE/Plugins/MomentumSlideSE.ini").string();
    enabled=GetPrivateProfileIntA("MomentumSlide","Enabled",1,path.c_str())!=0;
    debug=GetPrivateProfileIntA("MomentumSlide","DebugLogging",1,path.c_str())!=0;
    auto read=[&](const char* key,float& out,float low,float high) {
        char buffer[128]{}; GetPrivateProfileStringA("MomentumSlide",key,"",buffer,sizeof(buffer),path.c_str());
        if(!buffer[0]) return;
        try {
            std::string s(buffer); std::size_t end=0; float v=std::stof(s,&end);
            if(!std::isfinite(v) || s.find_first_not_of(" \t\r\n",end)!=std::string::npos) throw std::invalid_argument("number");
            out=std::clamp(v,low,high);
        } catch(...) {spdlog::warn("Invalid setting {}; retaining default {}",key,out);}
    };
    read("Friction",config.friction,0.1f,30); read("Drag",config.drag,0,5);
    read("SlopeGravity",config.gravity,0,40); read("MaxSpeed",config.maxSpeed,2,35);
    read("MinEntrySpeed",config.minEntrySpeed,0.5f,10);
    read("FlatAngle",config.flatAngle,0.5f,8);
    read("SustainAngle",config.sustainAngle,10,40);
    read("ContactGrace",config.contactGrace,0.05f,0.35f);
}
void OnMessage(SKSE::MessagingInterface::Message* m) {
    if(!m) return;
    if(m->type==SKSE::MessagingInterface::kPostPostLoad) {
        if(!GetModuleHandleW(L"SkyParkourNG.dll")) {spdlog::error("SkyParkourNG.dll missing; no hooks installed."); return;}
        // Install after plugins' Load/PostLoad hooks and chain every predecessor.
        REL::Relocation<std::uintptr_t> notify{RE::VTABLE_PlayerCharacter[3]};
        REL::Relocation<std::uintptr_t> player{RE::VTABLE_PlayerCharacter[0]};
        REL::Relocation<std::uintptr_t> clip{RE::VTABLE_hkbClipGenerator[0]};
        REL::Relocation<std::uintptr_t> ground{RE::VTABLE_bhkCharacterStateOnGround[0]};
        NotifyHook::original=notify.write_vfunc(1,&NotifyHook::Call);
        PlayerHook::original=player.write_vfunc(0xAD,&PlayerHook::Call);
        ClipHook::original=clip.write_vfunc(5,&ClipHook::Call);
        GroundHook::original=ground.write_vfunc(8,&GroundHook::Call);
        spdlog::info("MomentumSlideSE 0.3.1: smooth recovery, reconciled support, stock flat timing, {:.2f}s contact grace. Hooks installed for 1.6.1170.",config.contactGrace);
    } else if(m->type==SKSE::MessagingInterface::kDataLoaded) {
        static InputSink sink;
        if(auto* input=RE::BSInputDeviceManager::GetSingleton()) input->AddEventSink(&sink);
    } else if(m->type==SKSE::MessagingInterface::kPreLoadGame) {
        Stop(RE::PlayerCharacter::GetSingleton(),"loading save"); ready=false; Reset();
    } else if(m->type==SKSE::MessagingInterface::kNewGame || m->type==SKSE::MessagingInterface::kPostLoadGame) {
        Reset(); ready=true;
    }
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version=[] {
    SKSE::PluginVersionData info;
    info.PluginVersion({0,3,1,0}); info.PluginName("MomentumSlideSE"); info.AuthorName("MomentumSlideSE contributors");
    info.UsesAddressLibrary(); info.UsesStructsPost629(); info.CompatibleVersions({runtime}); return info;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if(skse->IsEditor() || skse->RuntimeVersion()!=runtime) return false;
    try {
        if(auto directory=SKSE::log::log_directory()) {
            auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>((*directory/"MomentumSlideSE.log").string(),true);
            spdlog::set_default_logger(std::make_shared<spdlog::logger>("MomentumSlideSE",sink));
            spdlog::flush_on(spdlog::level::info);
        }
        SKSE::Init(skse); ReadSettings();
        if(!enabled) {spdlog::info("Disabled; no hooks installed."); return true;}
        return SKSE::GetMessagingInterface() && SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    } catch(const std::exception& e) {spdlog::error("Initialization failed: {}",e.what()); return false;}
}
