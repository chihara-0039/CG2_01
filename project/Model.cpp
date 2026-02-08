#include "Model.h"
#include <fstream>
#include <sstream>
#include <cassert>

using namespace Microsoft::WRL;

Model* Model::CreateFromOBJ(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager) {
    Model* model = new Model();
    model->Initialize(dxCommon, directoryPath, filename, textureManager);
    return model;
}

void Model::Initialize(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager) {
    // 1. OBJ読み込み
    LoadObjFile(directoryPath, filename);
    // 2. バッファ生成
    CreateBuffers(dxCommon);
    // 3. テクスチャ読み込み (mtlファイル解析は省略し、今回は固定画像をロード)
    // ※実際は mtlファイルを読んでテクスチャ名を特定しますが、ここでは簡易的に uvChecker を使います
    if (textureManager) {
        // Resourcesフォルダにある前提。実際のパスに合わせて調整してください
        textureHandle_ = textureManager->LoadTexture("Resources/uvChecker.png");
    }
}

void Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string identifier;
        ss >> identifier;

        if (identifier == "v") {
            Vector4 position;
            ss >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            positions.push_back(position);
        } else if (identifier == "vt") {
            Vector2 texcoord;
            ss >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y; // OBJのUVはYが逆
            texcoords.push_back(texcoord);
        } else if (identifier == "vn") {
            Vector3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } else if (identifier == "f") {
            // 面データ (三角形のみ対応)
            for (int i = 0; i < 3; i++) {
                std::string s;
                ss >> s;
                std::stringstream ss2(s);
                std::string idx;
                int p = 0, t = 0, n = 0;

                // 書式: v/vt/vn
                std::getline(ss2, idx, '/'); if (!idx.empty()) p = std::stoi(idx) - 1;
                std::getline(ss2, idx, '/'); if (!idx.empty()) t = std::stoi(idx) - 1;
                std::getline(ss2, idx, '/'); if (!idx.empty()) n = std::stoi(idx) - 1;

                ModelVertexData v;
                v.position = positions[p];
                if (t >= 0 && t < texcoords.size()) v.texcoord = texcoords[t];
                if (n >= 0 && n < normals.size()) v.normal = normals[n];

                // 左手系に変換するためZ反転などが本来必要だが、今回は簡易的にそのまま
                v.position.z *= -1.0f; // 必要に応じて反転
                v.normal.z *= -1.0f;

                vertices_.push_back(v);
            }
            // 頂点順序を反転させる必要がある場合 (0, 2, 1) に入れ替える処理など
            // 今回はカリング設定次第なのでそのまま追加
        }
    }
}

void Model::CreateBuffers(DirectXCommon* dxCommon) {
    auto device = dxCommon->GetDevice();
    UINT sizeIB = static_cast<UINT>(sizeof(ModelVertexData) * vertices_.size());

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeIB;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_));

    // データ転送
    ModelVertexData* vertMap = nullptr;
    vertexBuffer_->Map(0, nullptr, (void**)&vertMap);
    std::copy(vertices_.begin(), vertices_.end(), vertMap);
    vertexBuffer_->Unmap(0, nullptr);

    // View作成
    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeIB;
    vertexBufferView_.StrideInBytes = sizeof(ModelVertexData);
}

void Model::Draw(ID3D12GraphicsCommandList* commandList) {
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
}