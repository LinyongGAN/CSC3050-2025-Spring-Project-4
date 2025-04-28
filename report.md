# CSC3050 Project 4: Cache Simulator Report

Group Members:   Linyong GAN, Yuxuan LIU                 
Student ID:      123090120, 123090377    
Contribution:    Yuxuan: part 1,2; Linyong: part 3, 4

Note: Only leave sections blank if they are not yet implemented. We will verify that your code matches the data in your report. Any inconsistencies will result in double point deductions.


## 1. Part 1


### 1.1 Split Cache Design

Description of how the split cache (ICache and DCache) is implemented and the main differences compared to the unified cache.

A split cache architecture separates Instruction Cache (ICache) and Data Cache (DCache), each handling instruction and data accesses independently. 

Split cache, separate storage and management for instructions and data, and thus instruction and data accesses can proceed in parallel.

In contrast, a unified cache combines instruction and data accesses into a single cache structure.

### 1.2 Implementation Details

Detailed explanation of key implementation methods, especially the design of the ICache class and the simulation method for parallel access.

**Icache Design:**
The ICache class inherits from the Cache class
Since instruction accesses are typically read-only and do not require actual data modification, 
the setByte() method in ICache is overridden to do nothing effectively:

```cpp
void setByte(uint32_t addr, uint8_t val, uint32_t *cycles = nullptr, bool countStats = true) override {
    if (cycles) {
        *cycles = 0;
    }
}
```

**Simulation method:**
First rewrite the function as `void simulateCache(std::ofstream &csvFile, bool isSplit);`
In the implementation, we build the unified/split cache inside the function according to the required policy.
In the simulateCache() function, when isSplit is true, accesses are dispatched based on the type field in the trace file:
Type 'I' accesses go to iCache. Type 'D' accesses go to dCache.
Instruction and data accesses are counted separately and managed independently.
When computing the total cycles for split cache, the maximum of instruction and data cache cycles is taken.

### 1.3 Result Analysis

| Trace File | Cache Type    | Miss Count | Expected Miss Count | Total Cycles | Expected Total Cycles |
| ---------- | ------------- | ---------- | ------------------- | ------------ | --------------------- |
| I.trace    | Split Cache   |  512       |     512             |  41088       |    41088              |
| I.trace    | Unified Cache |  384       |     384             |  54912       |    54912              |
| D.trace    | Split Cache   |  634       |     634             |  67584       |    67584              |
| D.trace    | Unified Cache |  507       |     507             |  68141       |    68141              |

Analysis of performance differences between split cache and unified cache for different trace files, explaining the observed results.

**I.trace:**
Split Cache shows a higher miss count (512) compared to Unified Cache (384), 
because the instruction cache in Split Cache has only half the total size, increasing the likelihood of misses.
However, Split Cache results in fewer total cycles (41088 vs 54912).
This is because instruction accesses do not compete with data accesses in Split Cache, enabling faster overall execution.
**D.trace:**
Similarly, Split Cache experiences more misses (634 vs 507) because of the smaller data cache.
However, total cycles are slightly lower for Split Cache (67584 vs 68141).

Even with a higher miss rate, Split Cache can process accesses more efficiently due to isolation from instruction accesses.

## 2. Part 2

### Correct results after bug fixed:

| Cache Level | Read Count | Write Count | Hit Count | Miss Count | Miss Rate | Total Cycles |
|-------|-----------|------------|----------|------------|-----------|--------------|
| L1    | 181,708   | 50,903     | 177,911  | 54,700     | 23.5%     | 717,519      |
| L2    | 47,667    | 7,033      | 25,574   | 29,126     | 53.1%     | 888,292      |
| L3    | 26,984    | 2,142      | 21,596   | 7,530      | 25.9%     | 1,184,920    |

### My Results after bug fixed:

| Cache Level | Read Count | Write Count | Hit Count | Miss Count  | Miss Rate | Total Cycles |
| ----------- | ---------- | ----------- | --------- | ----------- | --------- | ------------ |
| L1          |  181708    |  50903      |  177911   |  54700      |  23.5%    |  717519      |
| L2          |  /         |  /          |  25574    |  29126      |  53.3%    |  888292      |
| L3          |  /         |  /          |  21596    |  7530       |  25.9%    |  1184920     |


## 3. Part 3

### Optimization Techniques Application and Results
(Choose one optimization technique for each trace file)

*No Optimization* means, just use the initial code to run simulation.

| Trace       | Optimization    | Miss Count | Miss Rate | Expected Miss Rate |
| ----------- | --------------- | ---------- | --------- | ------------------ |
| test1.trace | No Optimization | 102656     | 99.26%    |       None         |
| test1.trace |    fifo         | 384        |  0.37%    |     0.371%         |
| test2.trace | No Optimization | 100013     | 98.53%    |       None         |
| test2.trace |    perfetching  | 1272       |  1.25%    |    5.803%          |
| test3.trace | No Optimization |            |           |       None         |
| test3.trace |  victim         |            |           |   0.372%           |


## 4. Part 4

### 4.1 Performance Comparison (L1 Level)

| Implementation | Read Count | Write Count | Hit Count | Miss Count | Miss Rate | Total Cycles |
|----------------|------------|-------------|-----------|------------|-----------|--------------|
| matmul0 |   786432      | 262144      |   716581      |    331995        |  31.6615       |   3665589      |
| matmul1 |   528384      |   4096      |   258741      |    273739        |  51.4083       |   2480901      |
| matmul2 |   786432      | 262144      |   933157      |    115419        |  11.0072       |   2378661      |
| matmul3 |   786432      | 262144      |   471776      |    576800        |  55.0079       |   7183072      |
| matmul4 |   548864      | 278528      |   819567      |      7825        |   0.945743     |    897495      |

### 4.2 Analysis of Performance Differences between matmul1, 2, 3

The matmul2 performs the best, following is matmul 1 and 3.

For matmul2, as the most inner loop variable is `j`, we could recognize `A[i*n+k]` as the temporal locality since `i*n+k` is an constant for a considerably period. Similarly, we could recognize `C[i*n+j]` and `B[k*n+j]` as spatial locality since these variables are located nearby with `j` increment one by one. 

For matmul1, there is one temporal locality for `C[i*n+j]` and one spatial locality for `A[i*n+k]`. Therefore, it is slower than matmul2. 

For matmul3, there is only one spatial locality for `B[k*n+j]`. Therefore, it is the slowest. 
