#include "FogExplorer.h"
#include <algorithm>
#include <cmath>

float FogExplorer::Smooth(float a, float b, float value) {
    const float t = std::clamp((value-a)/(b-a), 0.0f, 1.0f);
    return t*t*(3-2*t);
}
void FogExplorer::Reset() { mode_=2; age_=100; cooldown_=0; strength_=0; radius_=0; }
void FogExplorer::SetMode(int mode) { mode_=std::clamp(mode,0,2); }
bool FogExplorer::TryScan() {
    if (mode_!=2 || cooldown_>0) return false;
    age_=0; cooldown_=6; return true;
}
void FogExplorer::Update(float seconds, const Vector3& player, float cameraYaw, bool paused) {
    origin_=player;
    direction_={std::sin(cameraYaw),0,std::cos(cameraYaw)};
    if (!paused) {
        const float dt=std::clamp(seconds,0.0f,.1f);
        age_+=dt; cooldown_=(std::max)(0.0f,cooldown_-dt);
    }
    // 0.7秒で展開し、約2秒見せてから1秒で戻す。瞬間的な視界切替を避ける。
    strength_=Smooth(0,.35f,age_)*(1-Smooth(2.5f,3.5f,age_));
    radius_=36*Smooth(0,.9f,age_);
}
float FogExplorer::RevealAt(const Vector3& world) const {
    if (!mode_) return 0;
    const float x=world.x-origin_.x, z=world.z-origin_.z;
    const float radial=std::sqrt(x*x+z*z);
    const float local=1-Smooth(4,8,radial);
    if (mode_==1) return local;
    const float along=std::clamp(x*direction_.x+z*direction_.z,0.0f,28.0f);
    const float dx=x-direction_.x*along, dz=z-direction_.z*along;
    const float corridor=1-Smooth(4,8,std::sqrt(dx*dx+dz*dz));
    const float wave=1-Smooth(radius_-2,radius_+2,radial);
    return (std::max)(local,corridor*wave*strength_);
}
