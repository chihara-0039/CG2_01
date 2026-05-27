#pragma once
#include "SkinnedModel.h"
#include <string>
#include <vector>

class GltfLoader {
public:
    // glTFファイルから頂点、ウェイト、スケルトン、アニメーション、およびテクスチャ情報をロードする
    static bool LoadGltfModel(
        DirectXCommon* dxCommon,
        TextureManager* textureManager,
        const std::string& filePath,
        std::vector<ModelVertexData>& outVertices,
        std::vector<VertexInfluence>& outInfluences,
        std::vector<Joint>& outJoints,
        std::vector<MotionData>& outMotions,
        std::string& outTexturePath
    );
};
