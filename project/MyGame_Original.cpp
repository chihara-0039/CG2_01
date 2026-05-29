Created At: 2026-05-27T03:15:52Z
Completed At: 2026-05-27T03:15:52Z
File Path: `file:///c:/Users/CG2/generated/CG2_01/project/Game/Scene/MyGame.cpp`
Total Lines: 2085
Total Bytes: 89744
Showing lines 847 to 1646
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
847: void MyGame::UpdateImGui() {
848:     ImGuiIO& io = ImGui::GetIO();
849:     float panelWidth = 320.0f;
850:     float bottomHeight = 360.0f; // 荳九ヱ繝阪Ν縺ｮ繧ｵ繧､繧ｺ繧貞､ｧ縺阪￥縺励※繝斐ャ繧ｿ繝ｪ縺ｯ繧√ｋ
851: 
852:     // ==========================================
853:     // 1. 蟾ｦ繝代ロ繝ｫ (Information)
854:     // ==========================================
855:     ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
856:     ImGui::SetNextWindowSize(ImVec2(panelWidth, io.DisplaySize.y - bottomHeight), ImGuiCond_Always);
857:     ImGui::SetNextWindowBgAlpha(1.0f); // 騾城℃縺ｪ縺予r
858:     ImGui::Begin("Information", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
859: 
860:     ImGui::Text("FPS: %.1f (%.3f ms/f)", io.Framerate, 1000.0f / io.Framerate);
861:     ImGui::SameLine(panelWidth - 60.0f);
862:     if (ImGui::Button("Exit", ImVec2(50, 20))) {
863:         PostQuitMessage(0); // 繧｢繝励Μ繧ｱ繝ｼ繧ｷ繝ｧ繝ｳ邨ゆｺ・r
864:     }
865:     ImGui::Separator();
866: 
867:     if (ImGui::CollapsingHeader("Hierarchy / Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
868:         int modeIndex = 0;
869:         switch (currentMode_) {
870:         case AppMode::DebugView:      modeIndex = 0; break;
871:         case AppMode::StageEditor:    modeIndex = 1; break;
872:         case AppMode::GamePlay:       modeIndex = 2; break;
873:         case AppMode::SkinningEditor: modeIndex = 3; break;
874:         }
875: 
876:         const char* modeNames[] = { "DebugView", "StageEditor", "GamePlay", "SkinningEditor" };
877:         
<truncated 36887 bytes>

1605:                 if (terrainObject_ && debugFlags_.showTerrain) {
1606:                     terrainObject_->Draw();
1607:                 }
1608:                 for (auto& obj : objectList) {
1609:                     if (obj) obj->Draw();
1610:                 }
1611:                 if (player_) { player_->Draw(); }
1612:             }
1613:         }
1614:     }
1615: 
1616:     // 繝代・繝・ぅ繧ｯ繝ｫ縺ｮ謠冗判
1617:     if (debugFlags_.showParticles) {
1618:         ID3D12DescriptorHeap* particleHeaps[] = { textureManager->GetSrvHeap() };
1619:         commandList->SetDescriptorHeaps(1, particleHeaps);
1620:         particleManager->Draw();
1621:     }
1622: 
1623:     // 繧ｹ繝励Λ繧､繝医・謠冗判
1624:     if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
1625:         spriteCommon->PreDraw();
1626:         if (sprite) sprite->Draw();
1627:     }
1628: 
1629:     // UI繧ｹ繝励Λ繧､繝医・謠冗判
1630:     if (gameplayUIManager_) {
1631:         gameplayUIManager_->DrawSprites(
1632:             currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace,
1633:             gameplayCameraController_.IsFollowPlayerMode()
1634:         );
1635:     }
1636: 
1637:     // 繧､繝ｳ繝吶Φ繝医ΜUI縺ｮ謠冗判
1638:     if (blockInventoryUI_ && (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)) {
1639:         blockInventoryUI_->Draw();
1640:     }
1641: 
1642:     // 繝√Η繝ｼ繝医Μ繧｢繝ｫUI縺ｮ謠冗判
1643:     // 繧､繝ｳ繝吶Φ繝医Μ縺碁幕縺・※縺・ｋ縺ｨ縺搾ｼ・amePlay_BlockPlace・峨・驟咲ｽｮ繝√Η繝ｼ繝医Μ繧｢繝ｫ繧貞━蜈郁｡ｨ遉ｺ縺励―r
1644:     // 騾壼ｸｸ繧ｲ繝ｼ繝繝励Ξ繧､譎ゅ・縺ｿ謫堺ｽ懊メ繝･繝ｼ繝医Μ繧｢繝ｫ繧定｡ｨ遉ｺ縺吶ｋ・郁｢ｫ繧企亟豁｢・噂r
1645:     bool inventoryIsOpen = blockInventoryUI_ && blockInventoryUI_->IsActive();
1646: 
The above content does NOT show the entire file contents. If you need to view any lines of the file which were not shown to complete your task, call this tool again to view those lines.
