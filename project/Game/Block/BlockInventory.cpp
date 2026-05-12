#include "BlockInventory.h"

void BlockInventory::Initialize(int initialCount) {
    blockCount_ = initialCount;
}

void BlockInventory::AddBlock(int count) {
    if (count <= 0) {
        return;
    }

    blockCount_ += count;
}

bool BlockInventory::ConsumeBlock(int count) {
    if (count <= 0) {
        return false;
    }

    if (blockCount_ < count) {
        return false;
    }

    blockCount_ -= count;
    return true;
}