#include <cstdio>
#include <cstdlib>

#include "Cache.h"

#define PREFETCHING 1
#define FIFO 2
#define VICTIM 3

Cache::Cache(MemoryManager *manager, Policy policy, Cache *lowerCache, int tech, Cache *victimCache) {
  this->referenceCounter = 0;
  this->memory = manager;
  this->policy = policy;
  this->lowerCache = lowerCache;
	if (tech == VICTIM && victimCache != nullptr)	this->victimCache = victimCache;
  if (!this->isPolicyValid()) {
    fprintf(stderr, "Policy invalid!\n");
    exit(-1);
  }
  this->initCache();
  this->statistics.numRead = 0;
  this->statistics.numWrite = 0;
  this->statistics.numHit = 0;
  this->statistics.numMiss = 0;
  this->statistics.totalCycles = 0;

  this->tech = tech;
  this->previous_stride = 0;
  this->tripleSame = 0; this->tripleDiff = 0;
  this->previous_address = 0;
}

bool Cache::inCache(uint32_t addr) { // check whether cache hit
  return getBlockId(addr) != -1 ? true : false;
}

void Cache::handlePrefetching(uint32_t addr) {
    this->stride = addr - this->previous_address;
    if (this->stride == this->previous_stride)
    {
        this->tripleSame++;
        this->tripleDiff = 0;
    } else {
        this->tripleSame = 0;
        this->tripleDiff++;
    }

    previous_address = addr;
    previous_stride = stride;

    if (!this->is_prefetch && this->tripleSame >= 3)
    {
        this->is_prefetch = true;
        prefetch(addr+stride);
    }
    if (this->is_prefetch && this->tripleDiff >= 3)
    {
        this->is_prefetch = false;
    }
    if (this->is_prefetch) prefetch(addr+stride);
}

void Cache::prefetch(uint32_t addr)
{
    if(!this->memory->isPageExist(addr)) this->memory->addPage(addr);

    if (this->getBlockId(addr) != -1) return;
    this->loadBlockFromLowerLevel(addr, nullptr);
    return;
}

uint32_t Cache::getBlockId(uint32_t addr) {
	uint32_t tag = this->getTag(addr);
	uint32_t id = this->getId(addr);
	// printf("0x%x 0x%x 0x%x\n", addr, tag, id);
	// iterate over the given set
	for (uint32_t i = id * policy.associativity;
		i < (id + 1) * policy.associativity; ++i) {
		if (this->blocks[i].id != id) {
			fprintf(stderr, "Inconsistent ID in block %d: %d %d\n", i, this->blocks[i].id, id);
			exit(-1);
		}
		if (this->blocks[i].valid && this->blocks[i].tag == tag) {
			return i;
		}
	}
	return -1;
}

void Cache::insertBlock(const Block &block) {
	if (this->tech != VICTIM) return; // 确保当前是Victim Cache
  
	uint32_t setId = 0; // 全相联，只有一个组
	uint32_t begin = setId * this->policy.associativity;
	uint32_t end = (setId + 1) * this->policy.associativity;
	uint32_t replaceId = this->getReplacementBlockId(begin, end);
	Block replacedBlock = this->blocks[replaceId];
  
	if (replacedBlock.valid && replacedBlock.modified) {
	  this->writeBlockToLowerLevel(replacedBlock);
	}
  Block newB = block; newB.id = 0;
	this->blocks[replaceId] = newB;
	if (this->tech == FIFO || this->tech == VICTIM) {
	  FIFO_id.push(replaceId);
	}
}

uint8_t Cache::getByte(uint32_t addr, uint32_t *cycles, bool countStats) {
	if (countStats) {
		this->referenceCounter++;
		this->statistics.numRead++;
	}
 
	if (this->tech == PREFETCHING) handlePrefetching(addr);

	// [cache hit] If in cache, return directly
	int blockId = this->getBlockId(addr);
	if (blockId != -1) {
		uint32_t offset = this->getOffset(addr);
		if (countStats) {
			this->statistics.numHit++;
			this->statistics.totalCycles += this->policy.hitLatency;
			// this->blocks[blockId].lastReference = this->referenceCounter;
			if (cycles) *cycles = this->policy.hitLatency;
		}
		this->blocks[blockId].lastReference = this->referenceCounter;
		return this->blocks[blockId].data[offset];
	}
  
	if (tech == VICTIM && victimCache != nullptr) {
		// now, l1cache has already cache miss
		// there are two situations remain:
		int victimBlockId = this->victimCache->getBlockId(addr);
		if (victimBlockId != -1) { // 1. victim cache hit
			// swap this block and LRU block in l1cache
			Block victimBlock = victimCache->blocks[victimBlockId];
			victimCache->blocks[victimBlockId].valid = false;

			uint32_t setId = getId(addr);
			uint32_t begin = setId * policy.associativity;
			uint32_t end = (setId + 1) * policy.associativity;
			uint32_t replaceId = getReplacementBlockId(begin, end);
			Block replacedBlock = blocks[replaceId];

			if (replacedBlock.valid) {
				victimCache->insertBlock(replacedBlock);
			}

			blocks[replaceId] = victimBlock;
			blocks[replaceId].id = setId;
      blocks[replaceId].lastReference = referenceCounter;

			if (replacedBlock.modified) {
					writeBlockToLowerLevel(replacedBlock);
					statistics.totalCycles += policy.missLatency;
			}

			if (countStats) {
					statistics.numMiss--;
					statistics.numHit++;
					statistics.totalCycles += victimCache->policy.hitLatency + policy.hitLatency;
					if (cycles) *cycles = victimCache->policy.hitLatency + policy.hitLatency;
			}

      return blocks[replaceId].data[getOffset(addr)];

		} else { // 2. victim cache miss: load from lower cache
			if (countStats) {
				this->statistics.numMiss++;
				this->statistics.totalCycles += this->policy.missLatency;
				if (cycles) *cycles = this->policy.missLatency;
			}
			
			this->loadBlockFromLowerLevel(addr, cycles);
			blockId = this->getBlockId(addr);
			this->blocks[blockId].lastReference = this->referenceCounter;
			return blocks[blockId].data[this->getOffset(addr)];
		}
		

	} else {
  	// Else, find the data in memory or other level of cache
		if (countStats) {
			this->statistics.numMiss++;
			this->statistics.totalCycles += this->policy.missLatency;
			if (cycles) *cycles = this->policy.missLatency;
		}
  
		this->loadBlockFromLowerLevel(addr, cycles);
		blockId = this->getBlockId(addr);
		this->blocks[blockId].lastReference = this->referenceCounter;
		return blocks[blockId].data[this->getOffset(addr)];
	}

  // The block is in top level cache now, return directly
  // if ((blockId = this->getBlockId(addr)) != -1) {
  //   uint32_t offset = this->getOffset(addr);
  //   this->blocks[blockId].lastReference = this->referenceCounter;
  //   return this->blocks[blockId].data[offset];
  // } else {
  //   fprintf(stderr, "Error: data not in top level cache!\n");
  //   exit(-1);
  // }
}

void Cache::setByte(uint32_t addr, uint8_t val, uint32_t *cycles, bool countStats) {
  if (countStats) {
    this->referenceCounter++;
    this->statistics.numWrite++;
  }

  if (this->tech == PREFETCHING) handlePrefetching(addr);

  // [cache hit] If in cache, write to it directly
  int blockId = this->getBlockId(addr);
  if (blockId!= -1) {
    uint32_t offset = this->getOffset(addr);
    if (countStats) {
      this->statistics.numHit++;
      this->statistics.totalCycles += this->policy.hitLatency;
      // this->blocks[blockId].lastReference = this->referenceCounter;
      if (cycles) *cycles = this->policy.hitLatency;
    }
    this->blocks[blockId].modified = true;
    this->blocks[blockId].lastReference = this->referenceCounter;
    this->blocks[blockId].data[offset] = val;
    return;
  }

	if (tech == VICTIM && victimCache != nullptr) {
		// now, l1cache miss
		int victimBlockId = this->victimCache->getBlockId(addr);
		if (victimBlockId != -1) { // if victim cache hit
			Block victimBlock = this->victimCache->blocks[victimBlockId];
      this->victimCache->blocks[victimBlockId].valid = false;

			uint32_t setId = this->getId(addr);
      uint32_t begin = setId * this->policy.associativity;
      uint32_t end = (setId + 1) * this->policy.associativity;
      uint32_t replaceId = this->getReplacementBlockId(begin, end);
      Block replacedBlock = this->blocks[replaceId];

			if (replacedBlock.valid) {
        this->victimCache->insertBlock(replacedBlock);
      }

			this->blocks[replaceId] = victimBlock;
			this->blocks[replaceId].id = setId;
      this->blocks[replaceId].lastReference = this->referenceCounter;
      this->blocks[replaceId].modified = true; // 标记为修改
      this->blocks[replaceId].data[this->getOffset(addr)] = val;

			if (replacedBlock.modified) {
        this->writeBlockToLowerLevel(replacedBlock);
        this->statistics.totalCycles += this->policy.missLatency;
      }

			if (countStats) {
        this->statistics.numMiss--;  // 修正未命中计数
        this->statistics.numHit++;   // 视为主缓存命中
        uint32_t totalCycles = this->victimCache->policy.hitLatency + this->policy.hitLatency;
        this->statistics.totalCycles += totalCycles;
        if (cycles) *cycles = totalCycles;
      }
      return;
		}
	}

  // Else, load the data from cache
  if (countStats) {
    this->statistics.numMiss++;
    this->statistics.totalCycles += this->policy.missLatency;
    if (cycles) *cycles = this->policy.missLatency; 
  }
  
  this->loadBlockFromLowerLevel(addr, cycles);
  blockId = this->getBlockId(addr);
  this->blocks[blockId].modified = true;
  this->blocks[blockId].lastReference = this->referenceCounter;
  this->blocks[blockId].data[this->getOffset(addr)] = val;

  // if ((blockId = this->getBlockId(addr)) != -1) {
  //   uint32_t offset = this->getOffset(addr);
  //   this->blocks[blockId].modified = true;
  //   this->blocks[blockId].lastReference = this->referenceCounter;
  //   this->blocks[blockId].data[offset] = val;
  //   return;
  // } else {
  //   fprintf(stderr, "Error: data not in top level cache!\n");
  //   exit(-1);
  // }
}

void Cache::printInfo(bool verbose) {
  printf("---------- Cache Info -----------\n");
  printf("Cache Size: %d bytes\n", this->policy.cacheSize);
  printf("Block Size: %d bytes\n", this->policy.blockSize);
  printf("Block Num: %d\n", this->policy.blockNum);
  printf("Associativiy: %d\n", this->policy.associativity);
  printf("Hit Latency: %d\n", this->policy.hitLatency);
  printf("Miss Latency: %d\n", this->policy.missLatency);

  if (verbose) {
    for (int j = 0; j < this->blocks.size(); ++j) {
      const Block &b = this->blocks[j];
      printf("Block %d: tag 0x%x id %d %s %s (last ref %d)\n", j, b.tag, b.id,
             b.valid ? "valid" : "invalid",
             b.modified ? "modified" : "unmodified", b.lastReference);
      // printf("Data: ");
      // for (uint8_t d : b.data)
      // printf("%d ", d);
      // printf("\n");
    }
  }
}

void Cache::printStatistics() {
  printf("-------- STATISTICS ----------\n");
  printf("Num Read: %d\n", this->statistics.numRead);
  printf("Num Write: %d\n", this->statistics.numWrite);
  printf("Num Hit: %d\n", this->statistics.numHit);
  printf("Num Miss: %d\n", this->statistics.numMiss);
  
  float totalAccess = this->statistics.numHit + this->statistics.numMiss;
  float missRate = totalAccess > 0 ? (this->statistics.numMiss * 100.0f / totalAccess) : 0.0f;
  printf("Miss Rate: %.2f%%\n", missRate);
  
  printf("Total Cycles: %lu\n", this->statistics.totalCycles);
  if (this->lowerCache != nullptr) {
    printf("---------- LOWER CACHE ----------\n");
    this->lowerCache->printStatistics();
  }
}

bool Cache::isPolicyValid() {
  if (!this->isPowerOfTwo(policy.cacheSize)) {
    fprintf(stderr, "Invalid Cache Size %d\n", policy.cacheSize);
    return false;
  }
  if (!this->isPowerOfTwo(policy.blockSize)) {
    fprintf(stderr, "Invalid Block Size %d\n", policy.blockSize);
    return false;
  }
  if (policy.cacheSize % policy.blockSize != 0) {
    fprintf(stderr, "cacheSize %% blockSize != 0\n");
    return false;
  }
  if (policy.blockNum * policy.blockSize != policy.cacheSize) {
    fprintf(stderr, "blockNum * blockSize != cacheSize\n");
    return false;
  }
  if (policy.blockNum % policy.associativity != 0) {
    fprintf(stderr, "blockNum %% associativity != 0\n");
    return false;
  }
  return true;
}

void Cache::initCache() {
  this->blocks = std::vector<Block>(policy.blockNum);
  for (uint32_t i = 0; i < this->blocks.size(); ++i) {
    Block &b = this->blocks[i];
    b.valid = false;
    b.modified = false;
    b.size = policy.blockSize;
    b.tag = 0;
    b.id = i / policy.associativity;
    b.lastReference = 0;
    b.data = std::vector<uint8_t>(b.size);
  }
}

void Cache::loadBlockFromLowerLevel(uint32_t addr, uint32_t *cycles) {
  uint32_t blockSize = this->policy.blockSize;

  // Initialize new block from memory
  Block b;
  b.valid = true;
  b.modified = false;
  b.tag = this->getTag(addr);
  b.id = this->getId(addr);
  b.size = blockSize;
  b.data = std::vector<uint8_t>(b.size);
  uint32_t bits = this->log2i(blockSize);
  uint32_t mask = ~((1 << bits) - 1);
  uint32_t blockAddrBegin = addr & mask;
  for (uint32_t i = blockAddrBegin; i < blockAddrBegin + blockSize; ++i) {
    bool countStats = (i == blockAddrBegin);  
    if (this->lowerCache == nullptr) {
      b.data[i - blockAddrBegin] = this->memory->getByteNoCache(i);
      if (cycles) *cycles = 100;
    } else 
      b.data[i - blockAddrBegin] = this->lowerCache->getByte(i, cycles, countStats);
  }

  // Find replace block
  uint32_t id = this->getId(addr);
  uint32_t blockIdBegin = id * this->policy.associativity;
  uint32_t blockIdEnd = (id + 1) * this->policy.associativity;
  uint32_t replaceId = this->getReplacementBlockId(blockIdBegin, blockIdEnd);
  Block replaceBlock = this->blocks[replaceId];
  
  if(tech == FIFO) FIFO_id.push(replaceId);

  if (tech == VICTIM && victimCache != nullptr) {
		if (replaceBlock.valid) {
			if (this->victimCache != nullptr) {
				this->victimCache->insertBlock(replaceBlock);
			} else if (replaceBlock.modified) {
				this->writeBlockToLowerLevel(replaceBlock);
				this->statistics.totalCycles += this->policy.missLatency;
			}
		}
  } else {
		if (replaceBlock.valid && replaceBlock.modified) {
			this->writeBlockToLowerLevel(replaceBlock);
			this->statistics.totalCycles += this->policy.missLatency;
		}
	}

  this->blocks[replaceId] = b;
}

uint32_t Cache::getReplacementBlockId(uint32_t begin, uint32_t end) {
  // Find invalid block first
  for (uint32_t i = begin; i < end; ++i) {
    if (!this->blocks[i].valid)
      return i;
  }

  if (tech == FIFO)
  {
    uint32_t id = FIFO_id.front();
    FIFO_id.pop();
    return id;
  }

  // Otherwise use LRU
  uint32_t resultId = begin;
  uint32_t min = this->blocks[begin].lastReference;
  for (uint32_t i = begin; i < end; ++i) {
    if (this->blocks[i].lastReference < min) {
      resultId = i;
      min = this->blocks[i].lastReference;
    }
  }
  return resultId;
}

void Cache::writeBlockToLowerLevel(Cache::Block &b) {
  uint32_t addrBegin = getAddr(const_cast<Block&>(b));
  uint32_t tmpC;
  for (uint32_t i = 0; i < b.size; ++i) {
    uint32_t a = addrBegin + i;
    if (lowerCache) {
      lowerCache->setByte(a, b.data[i], &tmpC, false);
    } else {
      memory->setByteNoCache(a, b.data[i]);
    }
  }
}

bool Cache::isPowerOfTwo(uint32_t n) { return n > 0 && (n & (n - 1)) == 0; }

uint32_t Cache::log2i(uint32_t val) {
  if (val == 0)
    return uint32_t(-1);
  if (val == 1)
    return 0;
  uint32_t ret = 0;
  while (val > 1) {
    val >>= 1;
    ret++;
  }
  return ret;
}

uint32_t Cache::getTag(uint32_t addr) {
  uint32_t offsetBits = log2i(policy.blockSize);
  uint32_t idBits = log2i(policy.blockNum / policy.associativity);
  uint32_t mask = (1 << (32 - offsetBits - idBits)) - 1;
  return (addr >> (offsetBits + idBits)) & mask;
}

uint32_t Cache::getId(uint32_t addr) {
  uint32_t offsetBits = log2i(policy.blockSize);
  uint32_t idBits = log2i(policy.blockNum / policy.associativity);
  uint32_t mask = (1 << idBits) - 1;
  return (addr >> offsetBits) & mask;
}

uint32_t Cache::getOffset(uint32_t addr) {
  uint32_t bits = log2i(policy.blockSize);
  uint32_t mask = (1 << bits) - 1;
  return addr & mask;
}

uint32_t Cache::getAddr(Cache::Block &b) {
  uint32_t offsetBits = log2i(policy.blockSize);
  uint32_t idBits = log2i(policy.blockNum / policy.associativity);
  return (b.tag << (offsetBits + idBits)) | (b.id << offsetBits);
}

uint8_t Cache::read(uint32_t addr) {
    uint32_t cycles;
    return getByte(addr, &cycles, true);
}

void Cache::write(uint32_t addr, uint8_t val) {
    uint32_t cycles;
    setByte(addr, val, &cycles, true);
}
