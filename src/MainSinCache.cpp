#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include "Cache.h"
#include "Debug.h"
#include "MemoryManager.h"

bool parseParameters(int argc, char **argv);
void printUsage();
void simulateCache(std::ofstream &csvFile, bool isSplit);

bool verbose = false;
bool isSingleStep = false;
const char *traceFilePath;

class ICache : public Cache {
public:
  ICache(MemoryManager *manager, Policy policy, Cache *lowerCache = nullptr)
        : Cache(manager, policy, lowerCache) {}

  void setByte(uint32_t addr, uint8_t val, uint32_t *cycles = nullptr, bool countStats = true) override {
    if (cycles) {
      *cycles = 0;
    }
  }

  void write(uint32_t addr, uint8_t val) override {
        setByte(addr, val); 
  }

  virtual ~ICache() = default;
};

int main(int argc, char **argv) {
  if (!parseParameters(argc, argv)) {
    return -1;
  }

  // Open CSV file and write header
  std::ofstream csvFile(std::string(traceFilePath) + ".csv");
  csvFile << "cacheSize,blockSize,associativity,missRate,totalCycles\n";

  simulateCache(csvFile, false); 
  simulateCache(csvFile, true); 

  printf("Result has been written to %s\n",
         (std::string(traceFilePath) + ".csv").c_str());
  csvFile.close();
  return 0;
}

bool parseParameters(int argc, char **argv) {
  // Read Parameters
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'v':
        verbose = 1;
        break;
      case 's':
        isSingleStep = 1;
        break;
      default:
        return false;
      }
    } else {
      if (traceFilePath == nullptr) {
        traceFilePath = argv[i];
      } else {
        return false;
      }
    }
  }
  if (traceFilePath == nullptr) {
    return false;
  }
  return true;
}

void printUsage() {
  printf("Usage: CacheSim trace-file [-s] [-v]\n");
  printf("Parameters: -s single step, -v verbose output\n");
}

Cache::Policy createSingleLevelPolicy(uint32_t cacheSize,
                                     uint32_t blockSize,
                                     uint32_t associativity) {
    Cache::Policy policy;
    policy.cacheSize = cacheSize;
    policy.blockSize = blockSize;
    policy.blockNum = cacheSize / blockSize;
    policy.associativity = associativity;
    policy.hitLatency = 1;
    policy.missLatency = 100;
    return policy;
}

void simulateCache(std::ofstream &csvFile, bool isSplit) {
  MemoryManager *memory = new MemoryManager();
  std::string configStr; 
  Cache *dCache = nullptr; 
  ICache *iCache = nullptr; 

  if (isSplit) {
    uint32_t cacheSizeEach = 8 * 1024; 
    uint32_t blockSize = 64;
    uint32_t associativity = 1; 

    Cache::Policy iPolicy = createSingleLevelPolicy(cacheSizeEach, blockSize, associativity);
    Cache::Policy dPolicy = createSingleLevelPolicy(cacheSizeEach, blockSize, associativity);

    iCache = new ICache(memory, iPolicy); 
    dCache = new Cache(memory, dPolicy);  

    memory->setCache(iCache); 
    memory->setCache(dCache);
    iCache->printInfo(false);
    dCache->printInfo(false);
  } 
  else {
    uint32_t cacheSize = 16 * 1024; 
    uint32_t blockSize = 64;
    uint32_t associativity = 1; 

    Cache::Policy policy = createSingleLevelPolicy(cacheSize, blockSize, associativity);
    dCache = new Cache(memory, policy); 
    memory->setCache(dCache);
    dCache->printInfo(false);
  }

  // Read and execute trace in cache-trace/ folder
  std::ifstream trace(traceFilePath);
  if (!trace.is_open()) {
    printf("Unable to open file %s\n", traceFilePath);
    delete memory;
    delete dCache;
    if (iCache) delete iCache;
    exit(-1);
  }

  char op; //'r' for read, 'w' for write
  uint32_t addr;
  char type;
  uint64_t accessCount = 0;

  while (trace >> op >> std::hex >> addr >> type) {
    accessCount++;
    if (verbose) printf("Access %lu: %c 0x%x (%c)\n", accessCount, op, addr, type);
    if (!memory->isPageExist(addr)) memory->addPage(addr);
    if (isSplit) {
      if (type == 'I') {
        if (op == 'r') {
            iCache->getByte(addr);
        } else {
            iCache->setByte(addr, 0); 
        }
        if (verbose) iCache->printInfo(true); 
      } 
      else if (type == 'D') {
        if (op == 'r') {
          dCache->getByte(addr);
        } else if (op == 'w') {
          dCache->setByte(addr, 0);
        } else {
          dbgprintf("Illegal op '%c' in trace\n", op); 
          continue;
        }
        if (verbose) dCache->printInfo(true); 
      } 
      else {
        dbgprintf("Illegal type '%c' in trace\n", type);
        continue; 
      }
    } 
    else {
      if (op == 'r') {
          dCache->getByte(addr);
      } else if (op == 'w') {
          dCache->setByte(addr, 0);
      } else {
          dbgprintf("Illegal op '%c' in trace\n", op);
          continue; 
      }
      if (verbose) dCache->printInfo(true);
    }

    if (isSingleStep) {
      printf("Press Enter to Continue...");
      getchar();
    }
  }
  trace.close();

  if (isSplit) {
    printf("\n--- Split Cache Simulation Results ---\n");
    printf("Instruction Cache Statistics:\n");
    iCache->printStatistics();
    printf("\nData Cache Statistics:\n");
    dCache->printStatistics();

    uint64_t totalHits = iCache->statistics.numHit + dCache->statistics.numHit;
    uint64_t totalMisses = iCache->statistics.numMiss + dCache->statistics.numMiss;
    uint64_t totalAccesses = totalHits + totalMisses;
    uint64_t totalCycles = std::max(iCache->statistics.totalCycles, dCache->statistics.totalCycles);
    float combinedMissRate = (totalAccesses == 0) ? 0.0f : (float)totalMisses / totalAccesses;

    printf("\nCombined Split Cache Statistics:\n");
    printf("  Total Accesses: %llu\n", totalAccesses);
    printf("  Total Hits:     %llu\n", totalHits);
    printf("  Total Misses:   %llu\n", totalMisses);
    printf("  Miss Rate:      %.4f\n", combinedMissRate);
    printf("  Total Cycles:   %llu\n", totalCycles);

    uint32_t cacheSizeEach = 8 * 1024; 
    uint32_t totalCacheSize = cacheSizeEach * 2;
    uint32_t blockSize = 64;
    uint32_t associativity = 1; 

    csvFile << totalCacheSize << "," << blockSize << "," << associativity << ","
            << combinedMissRate << "," << totalCycles << std::endl;
  } 
  else {
    printf("\n--- Unified Cache Simulation Results ---\n");
    dCache->printStatistics();

    uint64_t totalHits = dCache->statistics.numHit;
    uint64_t totalMisses = dCache->statistics.numMiss;
    uint64_t totalAccesses = totalHits + totalMisses;
    uint64_t totalCycles = dCache->statistics.totalCycles;
    float missRate = (totalAccesses == 0) ? 0.0f : (float)totalMisses / totalAccesses;

    uint32_t cacheSize = 16 * 1024; 
    uint32_t blockSize = 64;
    uint32_t associativity = 1;

    csvFile << cacheSize << "," << blockSize << "," << associativity << ","
            << missRate << "," << totalCycles << std::endl;
  }

  delete dCache;
  if (iCache) delete iCache;
  delete memory;
}
