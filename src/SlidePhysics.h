#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>

namespace Slide {
struct Vec {
    float x{}, y{}, z{};
    Vec operator+(Vec b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec operator-(Vec b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec operator*(float b) const { return {x*b,y*b,z*b}; }
    float dot(Vec b) const { return x*b.x+y*b.y+z*b.z; }
    float length() const { return std::sqrt(dot(*this)); }
    bool finite() const { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z); }
};
struct Config {
    float friction{2.0f}; // m/s^2 of sliding resistance
    float drag{0.12f};    // 1/s, controls terminal downhill speed
    float gravity{10.0f};
    float maxSpeed{18.0f};
    float minEntrySpeed{2.0f};
    float flatAngle{3.0f};
    float sustainAngle{18.0f};
    float contactGrace{0.20f};
};
inline bool Normal(Vec n) { return n.finite() && n.length()>0.5f && n.length()<1.5f && n.z>0.1f; }
inline bool UnitNormal(Vec& n) {
    if(!Normal(n)) return false;
    n=n*(1/n.length());
    return true;
}
// Preserve cleanup across graph loss and local slide resets. Dispatch can call
// back into our hooks, so a nested retry must not deliver a duplicate event.
struct PendingCleanup {
    std::atomic_bool pending{}, dispatching{};
    void Request() { pending=true; }
    bool Pending() const { return pending.load(); }
    template<class Dispatch> void Retry(Dispatch dispatch) {
        if(!pending || dispatching.exchange(true)) return;
        if(dispatch()) pending=false;
        dispatching=false;
    }
};
inline Vec Tangent(Vec v, Vec n) { return v-n*v.dot(n); }
// Native on-ground state and a real support flag are stronger evidence than
// surfaceInfo's transient unsupported value during animation-driven movement.
inline bool HasContact(bool grounded,bool safe,bool normalValid,bool supportFlag,bool surfaceSupported,bool freshFloorProbe) {
    return grounded && safe && normalValid && (supportFlag || surfaceSupported || freshFloorProbe);
}
inline float PlaybackRate(float slopeRate,bool recovering) { return recovering ? 1.0f : slopeRate; }
// Alter only the middle of the stock clip. Entry, recovery and exit keep their
// original timing; on flat ground the entire input timestep passes through.
constexpr float middleStart=0.40f, middleEnd=0.70f, recovery=0.783333f;
inline float Rate(float downhillDegrees, const Config& c) {
    if (!std::isfinite(downhillDegrees)) return 1;
    const float amount=std::max(0.0f,std::abs(downhillDegrees)-c.flatAngle);
    const float fraction=amount/std::max(1.0f,c.sustainAngle-c.flatAngle);
    return downhillDegrees>0 ? std::clamp(1-fraction,0.0f,1.0f) : 1+std::min(fraction,2.0f);
}
inline float Advance(float time,float delta,float rate) {
    if (!std::isfinite(time)||!std::isfinite(delta)||delta<=0) return delta;
    if (rate==1 || time>=middleEnd) return delta;
    float remaining=delta, advanced=0;
    if(time<middleStart) {float n=std::min(remaining,middleStart-time);advanced+=n;remaining-=n;time+=n;}
    if(remaining>0 && time<middleEnd) {
        if(rate<=0) return advanced;
        float used=std::min(remaining,(middleEnd-time)/rate);
        advanced+=used*rate;remaining-=used;
    }
    return advanced+remaining;
}
struct Grace {
    float missing{};
    bool Update(bool supported,float dt,float limit) {
        if(supported) missing=0;
        else if(std::isfinite(dt)&&dt>0) missing+=dt;
        return missing>limit;
    }
};
inline float MotionWeight(float time) {
    if(time<middleStart || time>=recovery) return 0;
    return std::clamp((recovery-time)/(recovery-0.60f),0.0f,1.0f);
}
inline Vec Step(Vec actual, Vec normal, float dt, const Config& c) {
    if (!actual.finite() || !Normal(normal) || !std::isfinite(dt) || dt<=0 || dt>0.1f)
        return actual;
    normal=normal*(1/normal.length());
    Vec v=Tangent(actual,normal);
    const Vec acceleration=Tangent({0,0,-c.gravity},normal);
    // Substeps make stopping/uphill reversal robust at low frame rates.
    const int count=static_cast<int>(std::ceil(dt*240));
    const float h=dt/static_cast<float>(count);
    for (int i=0;i<count;++i) {
        v=v+acceleration*h;
        float speed=v.length();
        if (speed<=c.friction*h) return {};
        const float next=std::min(c.maxSpeed,(speed-c.friction*h)*std::exp(-c.drag*h));
        v=v*(next/speed);
    }
    return v;
}
// Only a fresh press after releasing the initiating press may cancel a slide.
struct CancelLatch {
    std::atomic_bool released{};
    void Start() { released=false; }
    bool Input(bool down, bool up) { if(up) released=true; return down&&released; }
};
}
