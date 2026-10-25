#pragma once
#include "StageMap.h"
#include <vector>
#include <cmath>

// 描画と確定が同じ回転後セル列・可否判定を使う。
namespace Placement {
enum class Reason { Ready, EmptyPart, Outside, Occupied, PlayerOverlap, NoInventory, Changed };
struct Cell { Int3 index; BlockType type; int variant; float rotationY; };
struct Plan { std::vector<Cell> cells; Reason reason=Reason::Ready; bool Ready() const {return reason==Reason::Ready;} };
inline Plan Make(const StageMap& map,Int3 origin,BlockType type,int customId,float rotationY,const Vector3* player=nullptr) {
    Plan plan;
    if(type==BlockType::None){plan.reason=Reason::EmptyPart;return plan;}
    int turns=static_cast<int>(std::round(rotationY/1.5707963f))%4;if(turns<0)turns+=4;
    const auto* part=customId>=1&&customId<=5?map.GetCustomPart(customId):nullptr;
    if(customId>=1&&customId<=5) {
        if(!part||part->IsEmpty()){plan.reason=Reason::EmptyPart;return plan;}
        for(int y=0;y<3;++y)for(int z=0;z<3;++z)for(int x=0;x<3;++x){
            const auto t=part->cells[y][z][x].type;if(t==BlockType::None)continue;
            int rx=x,rz=z;
            if(turns==1){rx=2-z;rz=x;}if(turns==2){rx=2-x;rz=2-z;}if(turns==3){rx=z;rz=2-x;}
            plan.cells.push_back({{origin.x+rx,origin.y+y,origin.z+rz},t,customId,turns*1.5707963f});
        }
    }else{
        const int variant=customId==0?(type==BlockType::Wall?6:type==BlockType::Ladder?7:0):customId;
        plan.cells.push_back({origin,type,variant,rotationY});
    }
    // 全セルを調べてから適用する。一部だけの配置を許さない。
    for(const auto& c:plan.cells){
        const auto* cell=map.GetCell(c.index);
        if(!cell){plan.reason=Reason::Outside;return plan;}
        if(cell->type!=BlockType::None){plan.reason=Reason::Occupied;return plan;}
        if(player && std::abs(player->x-c.index.x)<.89f && std::abs(player->z-c.index.z)<.89f &&
            player->y<c.index.y+.5f-.01f && player->y+1.8f>c.index.y-.5f+.01f){plan.reason=Reason::PlayerOverlap;return plan;}
    }
    return plan;
}
}
