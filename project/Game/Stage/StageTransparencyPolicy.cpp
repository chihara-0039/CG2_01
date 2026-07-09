#include "StageTransparencyPolicy.h"

bool StageTransparencyPolicy::IsTransparencyArea(int stageIndex, int x, int y, int z) {
    switch (stageIndex) {
    case 0:
        return false;
    case 1:
        return x >= 6 && x <= 15 &&
               y >= 1 && y <= 3 &&
               z == 6;
    default:
        return false;
    }
}
