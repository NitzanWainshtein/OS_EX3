# Q2 - Performance Comparison of Data Structures

## Description
Performance comparison between three implementations of the Convex Hull algorithm using different data structures:
- `std::vector` (from Q1)
- `std::deque` 
- `std::list`

## Project Files
```
q2/
├── convex_hull_vector.cpp    # Implementation with std::vector (from Q1)
├── convex_hull_deque.cpp     # Implementation with std::deque
├── convex_hull_list.cpp      # Implementation with std::list
├── Makefile                  # Build and profiling
└── README.md                 # This document
```

## Running the Experiment

### Build and Run:
```bash
make all                    # Build all executables
make profile               # Full profiling on 3 dataset sizes
```

### Datasets Tested:
- **Small**: 1,000 points
- **Medium**: 50,000 points  
- **Large**: 100,000 points

## Performance Results

### Execution Time (User Time)
| Dataset | Vector | Deque | List | 
|---------|--------|-------|------|
| Medium (50K) | **0.04s** | 0.05s | 0.05s |
| Large (100K) | **0.08s** | 0.10s | 0.15s |

### Memory Usage (maxresident KB)
| Dataset | Vector | Deque | List | Difference |
|---------|--------|-------|------|------------|
| Medium (50K) | **4,944** | 5,308 | 8,912 | Vector saves 45% vs List |
| Large (100K) | **6,552** | 7,116 | 14,428 | Vector saves 55% vs List |

### Page Faults (Memory Efficiency)
| Dataset | Vector | Deque | List |
|---------|--------|-------|------|
| Medium (50K) | **541** | 560 | 1,515 |
| Large (100K) | **930** | 971 | 2,883 |


## Recommendation

**For Convex Hull algorithm: Use `std::vector`**

**Reasoning:**
1. **Best performance**: Fastest execution time
2. **Memory efficient**: Lowest memory footprint
3. **Algorithm fit**: Convex Hull benefits from contiguous memory and efficient sorting
4. **Cache-friendly**: Contiguous data layout improves CPU cache utilization

## GProf Analysis Summary

The profiling revealed that:
- All implementations spend most time in `compare_points()` function (~2M calls)
- **Vector**: Efficient sorting with minimal overhead
- **Deque**: Some overhead in internal container operations
- **List**: Significant time spent on internal vector sorting (converts to vector for sorting)
