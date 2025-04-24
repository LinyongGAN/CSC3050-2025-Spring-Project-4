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

bool parseParameters(int argc, char **argv);
void printUsage();

const char *traceFilePath;

class CacheHierarchy {
private:
  MemoryManager* memory;
  Cache* l1cache;
  Cache* l2cache;
  Cache* l3cache;

public:
  CacheHierarchy() {
    memory = new MemoryManager();
    
    auto l1policy = MultiLevelCacheConfig::getL1Policy();
    auto l2policy = MultiLevelCacheConfig::getL2Policy();
    auto l3policy = MultiLevelCacheConfig::getL3Policy();
    
    l3cache = new Cache(memory, l3policy, nullptr);
    l2cache = new Cache(memory, l2policy, l3cache);
    l1cache = new Cache(memory, l1policy, l2cache);
    
    memory->setCache(l1cache);
  }
  
  ~CacheHierarchy() {
    delete l1cache;
    delete l2cache;
    delete l3cache;
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
  if (argc > 1) {
    traceFilePath = argv[1];
    return true;
  } else {
    return false;
  }
}

void printUsage() { printf("Usage: CacheSim trace-file\n"); }
