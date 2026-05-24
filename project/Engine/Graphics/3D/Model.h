#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "MyMath.h"
#include <string>
#include <vector>
#include <memory>

// 頂点データ構造体 (ShaderのInputLayoutに合わせる)
struct ModelVertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};


class Model {
public:
    // OBJファイルからモデル生成 (ディレクトリパスとファイル名を分けて渡す)
    static std::unique_ptr<Model> CreateFromOBJ(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager);

    void Initialize(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager);
    
    // 頂点配列から初期化する関数を追加 (OBJファイルを使わない動的モデル生成用)
    void InitializeFromVertices(DirectXCommon* dxCommon, const std::vector<ModelVertexData>& vertices, uint32_t textureHandle);
    
    // 頂点データを動的に更新する関数
    void UpdateVertexBuffer(const std::vector<ModelVertexData>& vertices);

    void Draw(ID3D12GraphicsCommandList* commandList);
    void DrawInstanced(ID3D12GraphicsCommandList* commandList, UINT instanceCount);

    // ゲッター
    uint32_t GetTextureHandle() const { return textureHandle_; }
    size_t GetVertexCount() const { return vertices_.size(); }

private:
    void LoadObjFile(const std::string& directoryPath, const std::string& filename);
    void CreateBuffers(DirectXCommon* dxCommon);
    std::string textureFilePath_ = "Resources/uvChecker.png";
private:
    std::vector<ModelVertexData> vertices_;
    uint32_t textureHandle_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	//マテリアル情報を保持する構造体とリスト
    void LoadMaterialFile(const std::string& directoryPath, const std::string& filename);
};