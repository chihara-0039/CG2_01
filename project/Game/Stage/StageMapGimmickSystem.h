#pragma once

class StageMap;

class StageMapGimmickSystem {
public:
    static void SetPSwitchActive(StageMap& stageMap, int switchId);
    static void ResetPSwitchStateNoRebuild(StageMap& stageMap);
    static void ResetPSwitchState(StageMap& stageMap);
    static void ToggleOnState(StageMap& stageMap);
};
