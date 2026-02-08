#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "MyMath.h"
#include <string>
#include <vector>

// 頂点データ構造体 (ShaderのInputLayoutに合わせる)
struct ModelVertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

class Model {
public:
    // OBJファイルからモデル生成 (ディレクトリパスとファイル名を分けて渡す)
    static Model* CreateFromOBJ(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager);

    void Initialize(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager);
    void Draw(ID3D12GraphicsCommandList* commandList);

    // ゲッター
    uint32_t GetTextureHandle() const { return textureHandle_; }

private:
    void LoadObjFile(const std::string& directoryPath, const std::string& filename);
    void CreateBuffers(DirectXCommon* dxCommon);

private:
    std::vector<ModelVertexData> vertices_;
    uint32_t textureHandle_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
};