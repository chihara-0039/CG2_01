#include "BlockInventory.h"

void BlockInventory::Initialize(int initialCount) {
    // 互換性のため、初期カウントは Wall に割り当てる
    wallCount_ = initialCount;
    ladderCount_ = 0;
    for (int i = 0; i < 5; ++i) {
        customCounts_[i] = 0;
    }
}

void BlockInventory::AddBlock(BlockType type, int count, int customId) {
    if (count <= 0) {
        return;
    }

    if (customId >= 1 && customId <= 5) {
        customCounts_[customId - 1] += count;
    } else {
        if (type == BlockType::Wall) {
            wallCount_ += count;
        } else if (type == BlockType::Ladder) {
            ladderCount_ += count;
        }
    }
}

void BlockInventory::AddBlock(int count) {
    AddBlock(BlockType::Wall, count, 0);
}

bool BlockInventory::ConsumeBlock(BlockType type, int count, int customId) {
    if (count <= 0) {
        return false;
    }

    if (customId >= 1 && customId <= 5) {
        if (customCounts_[customId - 1] >= count) {
            customCounts_[customId - 1] -= count;
            return true;
        }
    } else {
        if (type == BlockType::Wall) {
            if (wallCount_ >= count) {
                wallCount_ -= count;
                return true;
            }
        } else if (type == BlockType::Ladder) {
            if (ladderCount_ >= count) {
                ladderCount_ -= count;
                return true;
            }
        }
    }

    return false;
}

bool BlockInventory::ConsumeBlock(int count) {
    return ConsumeBlock(BlockType::Wall, count, 0);
}

int BlockInventory::GetBlockCount(BlockType type, int customId) const {
    if (customId >= 1 && customId <= 5) {
        return customCounts_[customId - 1];
    }

    if (type == BlockType::Wall) {
        return wallCount_;
    } else if (type == BlockType::Ladder) {
        return ladderCount_;
    }
    return 0;
}

int BlockInventory::GetBlockCount() const {
    int total = wallCount_ + ladderCount_;
    for (int i = 0; i < 5; ++i) {
        total += customCounts_[i];
    }
    return total;
}

bool BlockInventory::HasBlock(BlockType type, int customId) const {
    return GetBlockCount(type, customId) > 0;
}

bool BlockInventory::HasBlock() const {
    if (wallCount_ > 0 || ladderCount_ > 0) return true;
    for (int i = 0; i < 5; ++i) {
        if (customCounts_[i] > 0) return true;
    }
    return false;
}