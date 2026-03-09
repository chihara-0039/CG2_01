#include "StageMap.h"
#include <cassert>

void StageMap::Initialize(int width, int height, int depth) {
    assert(width > 0);
    assert(height > 0);
    assert(depth > 0);

    width_ = width;
    height_ = height;
    depth_ = depth;

    cells_.resize(width_ * height_ * depth_);
    Clear();
}

void StageMap::Clear() {
    for (MapCell& cell : cells_) {
        cell.type = BlockType::None;
        cell.variant = 0;
        cell.isSolid = false;
    }
}

bool StageMap::IsInside(int x, int y, int z) const {
    return
        x >= 0 && x < width_ &&
        y >= 0 && y < height_ &&
        z >= 0 && z < depth_;
}

bool StageMap::IsInside(const Int3& index) const {
    return IsInside(index.x, index.y, index.z);
}

const MapCell* StageMap::GetCell(int x, int y, int z) const {
    if (!IsInside(x, y, z)) {
        return nullptr;
    }
    return &cells_[ToIndex(x, y, z)];
}

const MapCell* StageMap::GetCell(const Int3& index) const {
    return GetCell(index.x, index.y, index.z);
}

MapCell* StageMap::GetCell(int x, int y, int z) {
    if (!IsInside(x, y, z)) {
        return nullptr;
    }
    return &cells_[ToIndex(x, y, z)];
}

MapCell* StageMap::GetCell(const Int3& index) {
    return GetCell(index.x, index.y, index.z);
}

bool StageMap::SetBlock(int x, int y, int z, BlockType type, int variant) {
    if (!IsInside(x, y, z)) {
        return false;
    }

    cells_[ToIndex(x, y, z)] = MakeCell(type, variant);
    return true;
}

bool StageMap::SetBlock(const Int3& index, BlockType type, int variant) {
    return SetBlock(index.x, index.y, index.z, type, variant);
}

bool StageMap::RemoveBlock(int x, int y, int z) {
    if (!IsInside(x, y, z)) {
        return false;
    }

    cells_[ToIndex(x, y, z)] = MakeCell(BlockType::None, 0);
    return true;
}

bool StageMap::RemoveBlock(const Int3& index) {
    return RemoveBlock(index.x, index.y, index.z);
}

int StageMap::ToIndex(int x, int y, int z) const {
    return x + (z * width_) + (y * width_ * depth_);
}

MapCell StageMap::MakeCell(BlockType type, int variant) {
    MapCell cell{};
    cell.type = type;
    cell.variant = variant;

    switch (type) {
    case BlockType::None:
    cell.isSolid = false;
    break;

    case BlockType::Ground:
    case BlockType::Wall:
    case BlockType::Stair:
    cell.isSolid = true;
    break;

    case BlockType::BubblePickup:
    case BlockType::Goal:
    case BlockType::PlayerStart:
    cell.isSolid = false;
    break;

    default:
    cell.isSolid = false;
    break;
    }

    return cell;
}