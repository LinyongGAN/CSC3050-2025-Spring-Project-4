#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Cache.h"
#include "Debug.h"
#include "MemoryManager.h"
#include "MultiLevelCacheConfig.h"

#define PREFETCHING 1
#define FIFO 2
#define VICTIM 3

bool parseParameters(int argc, char **argv);
void printUsage();

int tech = 0;
const char *traceFilePath;

static Cache::Policy getVictimPolicy() {
  Cache::Policy policy;
  policy.cacheSize = 8 * 1024;      // 16KB
  policy.blockSize = 64;
  policy.blockNum = policy.cacheSize / policy.blockSize;
  policy.associativity = policy.blockNum;
  policy.hitLatency = 1;
  policy.missLatency = 8;
  return policy;
}

class CacheHierarchy {
private:
  MemoryManager* memory;
  Cache* l1cache;
  Cache* l2cache;
  Cache* l3cache;
  Cache* victimCache;

public:
  CacheHierarchy() {
    memory = new MemoryManager();
    
    auto l1policy = MultiLevelCacheConfig::getL1Policy();
    auto l2policy = MultiLevelCacheConfig::getL2Policy();
    auto l3policy = MultiLevelCacheConfig::getL3Policy();
    Cache::Policy victimPolicy;
    if (tech == FIFO) l1policy.associativity = l1policy.blockNum;
    if (tech == VICTIM) victimPolicy = getVictimPolicy();

    l3cache = new Cache(memory, l3policy, nullptr, 0, nullptr);
    l2cache = new Cache(memory, l2policy, l3cache, 0, nullptr);
    if (tech != VICTIM) l1cache = new Cache(memory, l1policy, l2cache, tech, nullptr);
    else {
      victimCache = new Cache(memory, victimPolicy, nullptr, tech, nullptr);
      l1cache = new Cache(memory, l1policy, l2cache, tech, victimCache);
    }
    
    memory->setCache(l1cache);
  }
  
  ~CacheHierarchy() {
    delete l1cache;
    delete l2cache;
    delete l3cache;
    delete victimCache;
    delete memory;
  }
  
  void processMemoryAccess(char op, uint32_t addr) {
    if (!memory->isPageExist(addr)) {
      memory->addPage(addr);
    }
    
    switch (op) {
      case 'r':
        l1cache->read(addr);
        break;
      case 'w':
        l1cache->write(addr, 0);
        break;
      default:
        throw std::runtime_error("Illegal memory access operation");
    }
  }

  void printResults() const {
    printf("\n=== Cache Hierarchy Statistics ===\n");
    l1cache->printStatistics();
  }

  void outputResults() const {

    printResults();
    
    std::string csvPath = std::string(traceFilePath) + "_multi_level.csv";
    std::ofstream csvFile(csvPath);
    
    csvFile << "Level,NumReads,NumWrites,NumHits,NumMisses,MissRate,TotalCycles\n";

    // modified
    outputCacheStats(csvFile, "L1", l1cache);
    if (tech == VICTIM) outputCacheStats(csvFile, "victim", victimCache);
    outputCacheStats(csvFile, "L2", l2cache);
    outputCacheStats(csvFile, "L3", l3cache);

    csvFile.close();
    printf("\nResults have been written to %s\n", csvPath.c_str());
  }
private:
  void outputCacheStats(std::ofstream& csvFile, const char* level, const Cache* cache) const {
    // modified
    if (!cache) {
      fprintf(stderr, "Error: outputCacheStats called with null cache pointer for level %s\n", level);
      return;
    }
    auto& stats = cache->statistics;
    float missRate = static_cast<float>(stats.numMiss) / 
                    (stats.numHit + stats.numMiss) * 100;
    
    csvFile << level << ","
            << stats.numRead << ","
            << stats.numWrite << ","
            << stats.numHit << ","
            << stats.numMiss << ","
            << missRate << ","
            << stats.totalCycles << "\n";
  }
};

int main(int argc, char **argv) {
  if (!parseParameters(argc, argv)) {
    printUsage();
    return -1;
  }

  std::ifstream trace(traceFilePath);
  if (!trace.is_open()) {
    printf("Unable to open file %s\n", traceFilePath);
    return -1;
  }

  try {
    CacheHierarchy cacheHierarchy;
    char op;
    uint32_t addr;
   // cacheHierarchy.l1cache->stride = 0; cacheHierarchy.l1cache->is_prefetch = false;
    while (trace >> op >> std::hex >> addr) {
      cacheHierarchy.processMemoryAccess(op, addr);
    }
    
    cacheHierarchy.outputResults();
  } 
  catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return -1;
  }

  return 0;
}

bool parseParameters(int argc, char **argv) {
  // Read Parameters
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'p':
        tech = PREFETCHING;
        break;
      case 'f':
        tech = FIFO;
        break;
      case 'v':
        tech = VICTIM;
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

void printUsage() { printf("Usage: CacheSim trace-file\n"); }
