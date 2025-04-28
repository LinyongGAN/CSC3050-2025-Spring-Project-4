#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <vector>
#include "MemoryManager.h"
#include <queue>

class MemoryManager;

class ICacheAccess {
public:
    virtual uint8_t read(uint32_t addr) = 0;
    virtual void write(uint32_t addr, uint8_t val) = 0;
    virtual ~ICacheAccess() = default;
};

using CacheLevelConfig = struct {
    uint32_t size;
    uint32_t blockSize;
    uint32_t associativity;
    uint32_t hitLatency;
    uint32_t missLatency;
};

class Cache : public ICacheAccess {
public:
    struct Policy {
        // In bytes, must be power of 2
        uint32_t cacheSize;
        uint32_t blockSize;
        uint32_t blockNum;
        uint32_t associativity;
        uint32_t hitLatency;
        uint32_t missLatency;
    };

    struct Block {
        bool valid;
        bool modified;
        uint32_t tag;
        uint32_t id;
        uint32_t size;
        uint32_t lastReference;
        std::vector<uint8_t> data;
        Block() {}
        Block(const Block &b)
        : valid(b.valid), modified(b.modified), tag(b.tag), id(b.id),
            size(b.size) {
        data = b.data;
        }
    };

    struct Statistics {
        uint32_t numRead;
        uint32_t numWrite;
        uint32_t numHit;
        uint32_t numMiss;
        uint64_t totalCycles;
    };

    Cache(MemoryManager *manager, Policy policy, Cache *lowerCache = nullptr, int tech=0, Cache *victimCache=nullptr);

    bool inCache(uint32_t addr);
    uint32_t getBlockId(uint32_t addr);
    uint8_t getByte(uint32_t addr, uint32_t *cycles = nullptr, bool countStats = true);
    virtual void setByte(uint32_t addr, uint8_t val, uint32_t *cycles = nullptr, bool countStats = true);

    void printInfo(bool verbose);
    void printStatistics();
    Statistics statistics;

    uint8_t read(uint32_t addr) override;
    void write(uint32_t addr, uint8_t val) override;

private:
    uint32_t referenceCounter;
    MemoryManager *memory;
    Cache *lowerCache;
    Cache *victimCache;
    Policy policy;
    std::vector<Block> blocks;
    int tech;
    uint32_t previous_stride, previous_address, stride=0;
    bool is_prefetch = false;
    int tripleSame, tripleDiff;

    std::queue<uint32_t> FIFO_id;

    void handlePrefetching(uint32_t addr);
    void prefetch(uint32_t addr);
    void initCache();
    void insertBlock(const Block &replaceBlock);
    void loadBlockFromLowerLevel(uint32_t addr, uint32_t *cycles);
    uint32_t getReplacementBlockId(uint32_t begin, uint32_t end);
    void writeBlockToLowerLevel(Block &b);
    // Utility Functions
    bool isPolicyValid();
    bool isPowerOfTwo(uint32_t n);
    uint32_t log2i(uint32_t val);
    uint32_t getTag(uint32_t addr);
    uint32_t getId(uint32_t addr);
    uint32_t getOffset(uint32_t addr);
    uint32_t getAddr(Block &b);

};

#endif