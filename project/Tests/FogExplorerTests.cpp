#include "FogExplorer.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void Check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
int main() {
    FogExplorer fog; fog.Reset();
    fog.SetMode(0); fog.Update(0,{0,0,0},0,false);
    Check(fog.RevealAt({0,0,0})==0,"baseline must not reveal");
    fog.SetMode(1);
    Check(fog.RevealAt({0,0,0})==1,"local player visibility");
    Check(fog.RevealAt({0,0,18})==0,"local reveal must not expose distant routes");
    Check(!fog.TryScan(),"bubble comparison must not use scan");
    fog.SetMode(2); Check(fog.TryScan(),"initial scan must be ready");
    Check(!fog.TryScan(),"repeated input must respect cooldown");
    for(int i=0;i<60;++i) fog.Update(1.0f/60,{0,0,0},0,false);
    Check(fog.RevealAt({0,0,22})>.99f,"scan must reveal forward route");
    Check(fog.RevealAt({22,0,0})<.01f,"scan must not expose side route until camera turns");
    const float cooldown=fog.Cooldown();
    for(int i=0;i<200;++i) fog.Update(1.0f/60,{0,0,0},1.5707963268f,true);
    Check(std::abs(cooldown-fog.Cooldown())<.0001f,"pause must freeze gameplay timer");
    Check(fog.RevealAt({22,0,0})>.99f,"camera rotation must redirect the scan");
    for(int i=0;i<180;++i) fog.Update(1.0f/60,{0,0,0},0,false);
    Check(fog.RevealAt({0,0,22})<.01f,"temporary reveal must fade back");
    for(int i=0;i<240;++i) fog.Update(1.0f/60,{0,0,0},0,false);
    Check(fog.TryScan(),"scan must recharge");
    for(int mode=0;mode<=2;++mode) {
        fog.SetMode(mode);
        for(int i=0;i<240;++i) {
            fog.Update(1.0f/60,{0,0,0},i*.01f,false);
            for(int x=-40;x<=40;++x) {
                float a=fog.RevealAt({float(x),0,20});
                float b=fog.RevealAt({x+.001f,0,20});
                Check(a>=0 && a<=1 && std::isfinite(a),"mask must stay finite and bounded");
                Check(std::abs(a-b)<.01f,"mask boundaries must be continuous");
            }
        }
    }
    std::cout<<"FogExplorer: cooldown, pause, rotation, decay and 58320 spatial samples passed\n";
}
