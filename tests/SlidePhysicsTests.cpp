#include "SlidePhysics.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace Slide;
void Check(bool ok,const char* message) { if(!ok) {std::cerr<<message<<'\n';std::exit(1);} }
float Duration(float rate,int hz,float playback=1) {
    float time=0, elapsed=0;
    for(int i=0;i<hz*20;++i) {
        time+=Advance(time,playback/hz,rate);elapsed+=1.0f/hz;
        if(time>=0.85f) return elapsed;
    }
    return elapsed;
}
int main() {
    Config c;
    for(Vec invalid : {Vec{}, Vec{0,0,0.1f}, Vec{0,0,-1},
        Vec{0,0,std::numeric_limits<float>::infinity()},
        Vec{0,0,std::numeric_limits<float>::quiet_NaN()}}) {
        Check(!UnitNormal(invalid),"Missing or corrupt terrain normal must never be normalized");
    }
    Vec valid{0,0.6f,0.8f};
    Check(UnitNormal(valid) && valid.finite() && std::abs(valid.length()-1)<0.00001f,
        "Valid captured terrain sample must remain usable without a second contact query");
    PendingCleanup cleanup;
    int delivered=0;
    cleanup.Request();
    for(int i=0;i<100;++i) cleanup.Retry([] { return false; });
    Check(cleanup.Pending(),"Missing graph must retain cleanup across update retries");
    cleanup.Retry([&] {
        cleanup.Retry([&] { ++delivered; return true; });
        ++delivered; return true;
    });
    cleanup.Retry([&] { ++delivered; return true; });
    Check(!cleanup.Pending() && delivered==1,"Restored graph must receive exactly one cleanup, even with reentrant hooks");
    cleanup.Request();
    cleanup.Retry([&] { ++delivered; return true; });
    Check(delivered==2 && !cleanup.Pending(),"A later interrupted slide must also clean up");
    std::cout<<"PASS: invalid terrain normals; deferred cleanup, graph restoration and reentrant dispatch.\n";
    for(int hz:{30,60,144}) for(float playback:{0.75f,1.0f,1.5f}) {
        const float delta=playback/hz;
        for(float time:{0.0f,0.39f,0.40f,0.50f,0.69f,0.70f,0.84f})
            Check(Advance(time,delta,Rate(0,c))==delta,"Flat ground must pass exact stock animation time");
        float base=Duration(1,hz,playback);
        Check(std::abs(base-0.85f/playback)<=1.01f/hz,"Flat duration must equal stock stop time");
        Check(Duration(Rate(-20,c),hz,playback)<base,"Uphill must shorten stock duration");
        Check(Duration(Rate(10,c),hz,playback)>base,"Gentle downhill must lengthen stock duration");
    }
    Check(Rate(2,c)==1&&Rate(-2,c)==1,"Minor terrain noise must stay in flat deadzone");
    Check(Rate(20,c)==0,"Steep downhill must sustain middle pose");
    float t=0;
    for(int i=0;i<120*144;++i) t+=Advance(t,1.0f/144,Rate(25,c));
    Check(std::abs(t-middleStart)<0.0001f,"Long downhill must not reach stop event");
    float tail=0;
    while(t<0.85f) {t+=Advance(t,1.0f/144,1);tail+=1.0f/144;}
    Check(tail>0.44f&&tail<0.47f,"Downhill-to-flat must play remaining recovery, not stop suddenly");
    Check(std::abs(Advance(0.39f,0.1f,0)-0.01f)<0.00001f,"Boundary crossing must hold at middle, not overshoot");
    Check(std::abs(Advance(0.69f,0.1f,2)-0.105f)<0.00001f,"End of middle must resume normal time within same frame");
    Check(MotionWeight(0.2f)==0&&MotionWeight(0.85f)==0,"Entry/exit must retain native movement");
    Check(MotionWeight(0.7f)>0&&MotionWeight(0.7f)<1,"Momentum must blend into recovery");
    Grace grace;
    Check(!grace.Update(false,0.016f,c.contactGrace),"One-frame contact loss must not cancel");
    Check(!grace.Update(false,0.10f,c.contactGrace),"Short terrain gaps must not cancel");
    Check(!grace.Update(true,0.016f,c.contactGrace)&&grace.missing==0,"Recontact must reset grace");
    Check(grace.Update(false,0.21f,c.contactGrace),"True departure must still end slide");
    // Actual 0.2.0 failure signature: state=on-ground, support flag=true,
    // surfaceInfo=unsupported. This must remain a valid contact.
    Check(HasContact(true,true,true,true,false,false),"Native support flag must override surfaceInfo unsupported");
    Check(HasContact(true,true,true,false,true,false),"Native supported/sliding surface must be accepted");
    Check(HasContact(true,true,true,false,false,true),"Fresh nearby floor probe must resolve inconclusive contact");
    Check(!HasContact(true,true,true,false,false,false),"Stale normals alone must not invent ground");
    Check(!HasContact(false,true,true,true,true,true),"Nearby floor must not override a genuinely airborne controller");
    Check(!HasContact(true,false,true,true,true,true),"No-sim/ragdoll must never acquire contact");
    Check(!HasContact(true,true,false,true,true,true),"Invalid normal must not acquire contact");
    for(int hz:{30,60,144}) {
        float time=middleStart, duration=0;
        for(int i=0;i<hz && time<0.85f;++i) {
            // Continued steep downhill must not re-freeze a requested recovery.
            time+=Advance(time,1.0f/hz,PlaybackRate(Rate(45,c),true)); duration+=1.0f/hz;
        }
        Check(time>=0.85f && duration>=0.44f && duration<0.49f,"Cancellation must play remaining tail smoothly even downhill");
        Check(PlaybackRate(0,false)==0,"Normal downhill holding must remain available");
    }
    const Vec down{0,0.5f,0.8660254f};
    Vec v=Tangent({0,7,0},down);
    auto next=Step(v,down,1.0f/60,c);
    Check(next.length()>v.length(),"Downhill physical acceleration missing");
    for(int i=0;i<120*60;++i) v=Step(v,down,1.0f/60,c);
    Check(v.finite()&&v.length()<=c.maxSpeed+0.001f,"Downhill speed must remain bounded");
    Check(Step({}, {0,0,1},0.016f,c).length()==0,"Zero collision speed must not resurrect cached momentum");
    auto spike=Step({0,7,0},down,0.5f,c);
    Check(spike.y==7,"Frame spike must leave velocity untouched, not terminate slide");
    CancelLatch latch;latch.Start();
    Check(!latch.Input(true,false),"Initiating press must not cancel");
    Check(!latch.Input(false,true)&&latch.Input(true,false),"Release then second press must cancel");
    std::cout<<"PASS: stock flat timing; uphill/downhill durations; 120s sustained slope; smooth cancellation recovery; native support vs surfaceInfo conflict; fresh floor evidence; airborne/ragdoll rejection; contact grace; momentum cap.\n";
}
