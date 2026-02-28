# Benchmarks

### Types of benchmarks

- Set() | Without RCU | Without copying
- Set() | Without RCU | With copying
- Set() | With RCU | With copying
- Set() | With RCU | With optimized copying


Google Benchmark (Executing time)
```bash
Setting 1x1x1 voxels
-------------------------------------------------------------------------------
Benchmark                                     Time             CPU   Iterations
-------------------------------------------------------------------------------
BM_SVO_Set_With_RCU_With_Clone_Check       38.9 ns         38.5 ns     18218994
BM_SVO_Set_With_RCU                        36.3 ns         36.2 ns     18994190
BM_SVO_Set_With_Copy                       30.9 ns         30.8 ns     22754066
BM_SVO_Set_Without_Copy                    6.63 ns         6.63 ns    101695059

Setting 4x4x4 voxels
-------------------------------------------------------------------------------
Benchmark                                     Time             CPU   Iterations
-------------------------------------------------------------------------------
BM_SVO_Set_With_RCU_With_Clone_Check       4559 ns         4550 ns       155066
BM_SVO_Set_With_RCU                        5744 ns         5723 ns       119391
BM_SVO_Set_With_Copy                       4797 ns         4797 ns       147496
BM_SVO_Set_Without_Copy                    3325 ns         3324 ns       207965

Setting 64x64x64 voxels
-------------------------------------------------------------------------------
Benchmark                                     Time             CPU   Iterations
-------------------------------------------------------------------------------
BM_SVO_Set_With_RCU_With_Clone_Check   35451295 ns     35363355 ns           21
BM_SVO_Set_With_RCU                    75552000 ns     75368147 ns           13
BM_SVO_Set_With_Copy                   22358612 ns     22329868 ns           32
BM_SVO_Set_Without_Copy                15479665 ns     15456510 ns           48
```

Perf stat
```bash
BM_SVO_Set_Without_Copy
42,861,229  cpu_atom/branches/                                                    
68,434      cpu_atom/branch-misses/          #    0.16% of all

BM_SVO_Set_With_Copy
70,892,311  cpu_atom/branches/                                                    
285,961     cpu_atom/branch-misses/          #    0.40% of all branches

BM_SVO_Set_With_RCU
131,837,525  cpu_atom/branches/                                                    
328,187      cpu_atom/branch-misses/         #    0.25% of all branches

BM_SVO_Set_With_RCU_With_Clone_Check
78,319,039   cpu_atom/branches/                                                    
435,224      cpu_atom/branch-misses/         #    0.56% of all branches       
```

Memory usage
```bash
BM_SVO_Set_Without_Copy
Memory:       24.0 MB
Allocations:  299,592
```