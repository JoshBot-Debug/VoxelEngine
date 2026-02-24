With Clone Check
```bash
joshua@HQ-SYS-C66:~/TMP/VoxelEngine-main$ ./build/experiment --benchmark_out=baseline.json --benchmark_out_format=json --benchmark_repetitions=20
2026-02-24T14:41:12+05:30
Running ./build/experiment
Run on (4 X 3194.52 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x4)
  L1 Instruction 32 KiB (x4)
  L2 Unified 256 KiB (x4)
  L3 Unified 8192 KiB (x1)
Load Average: 0.60, 0.86, 0.96
---------------------------------------------------------------------
Benchmark                           Time             CPU   Iterations
---------------------------------------------------------------------
BM_SVO_Set_With_RCU               116 ns          116 ns      5980994
BM_SVO_Set_With_RCU              86.6 ns         86.6 ns      5980994
BM_SVO_Set_With_RCU              73.8 ns         73.8 ns      5980994
BM_SVO_Set_With_RCU              73.5 ns         73.5 ns      5980994
BM_SVO_Set_With_RCU              86.3 ns         86.3 ns      5980994
BM_SVO_Set_With_RCU              73.8 ns         73.8 ns      5980994
BM_SVO_Set_With_RCU              85.6 ns         85.6 ns      5980994
BM_SVO_Set_With_RCU              85.7 ns         85.7 ns      5980994
BM_SVO_Set_With_RCU              86.0 ns         86.0 ns      5980994
BM_SVO_Set_With_RCU              74.1 ns         74.1 ns      5980994
BM_SVO_Set_With_RCU              85.4 ns         85.4 ns      5980994
BM_SVO_Set_With_RCU              86.5 ns         85.9 ns      5980994
BM_SVO_Set_With_RCU              85.5 ns         85.5 ns      5980994
BM_SVO_Set_With_RCU              86.1 ns         86.0 ns      5980994
BM_SVO_Set_With_RCU              85.5 ns         85.5 ns      5980994
BM_SVO_Set_With_RCU              85.7 ns         85.7 ns      5980994
BM_SVO_Set_With_RCU              85.1 ns         85.0 ns      5980994
BM_SVO_Set_With_RCU              73.8 ns         73.8 ns      5980994
BM_SVO_Set_With_RCU              85.5 ns         85.5 ns      5980994
BM_SVO_Set_With_RCU              85.4 ns         85.3 ns      5980994
BM_SVO_Set_With_RCU_mean         84.3 ns         84.2 ns           20
BM_SVO_Set_With_RCU_median       85.5 ns         85.5 ns           20
BM_SVO_Set_With_RCU_stddev       9.14 ns         9.13 ns           20
BM_SVO_Set_With_RCU_cv          10.84 %         10.84 %            20
```

Without Clone Check
```bash
joshua@HQ-SYS-C66:~/TMP/VoxelEngine-main$ ./build/experiment --benchmark_out=contender.json --benchmark_out_format=json --benchmark_repetitions=20
2026-02-24T14:42:13+05:30
Running ./build/experiment
Run on (4 X 3192.61 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x4)
  L1 Instruction 32 KiB (x4)
  L2 Unified 256 KiB (x4)
  L3 Unified 8192 KiB (x1)
Load Average: 0.94, 0.95, 0.99
---------------------------------------------------------------------
Benchmark                           Time             CPU   Iterations
---------------------------------------------------------------------
BM_SVO_Set_With_RCU               113 ns          113 ns      6143745
BM_SVO_Set_With_RCU              83.9 ns         83.9 ns      6143745
BM_SVO_Set_With_RCU              71.7 ns         71.7 ns      6143745
BM_SVO_Set_With_RCU              71.7 ns         71.7 ns      6143745
BM_SVO_Set_With_RCU              83.9 ns         83.9 ns      6143745
BM_SVO_Set_With_RCU              71.6 ns         71.6 ns      6143745
BM_SVO_Set_With_RCU              83.4 ns         83.4 ns      6143745
BM_SVO_Set_With_RCU              83.3 ns         83.3 ns      6143745
BM_SVO_Set_With_RCU              83.3 ns         83.3 ns      6143745
BM_SVO_Set_With_RCU              71.9 ns         71.9 ns      6143745
BM_SVO_Set_With_RCU              83.6 ns         83.6 ns      6143745
BM_SVO_Set_With_RCU              83.8 ns         83.8 ns      6143745
BM_SVO_Set_With_RCU              83.8 ns         83.8 ns      6143745
BM_SVO_Set_With_RCU              83.3 ns         83.3 ns      6143745
BM_SVO_Set_With_RCU              83.7 ns         83.7 ns      6143745
BM_SVO_Set_With_RCU              83.8 ns         83.8 ns      6143745
BM_SVO_Set_With_RCU              83.6 ns         83.6 ns      6143745
BM_SVO_Set_With_RCU              71.6 ns         71.6 ns      6143745
BM_SVO_Set_With_RCU              83.7 ns         83.7 ns      6143745
BM_SVO_Set_With_RCU              83.6 ns         83.6 ns      6143745
BM_SVO_Set_With_RCU_mean         82.1 ns         82.1 ns           20
BM_SVO_Set_With_RCU_median       83.6 ns         83.6 ns           20
BM_SVO_Set_With_RCU_stddev       9.00 ns         9.00 ns           20
BM_SVO_Set_With_RCU_cv          10.96 %         10.96 %            20
```

Comparison: 
Without the clone check, the algorithm is ~2.6% faster
```bash
joshua@HQ-SYS-C66:~/TMP/VoxelEngine-main$ python3 ~/benchmark/tools/compare.py benchmarks baseline.json contender.json
Comparing baseline.json to contender.json
Benchmark                                    Time             CPU      Time Old      Time New       CPU Old       CPU New
-------------------------------------------------------------------------------------------------------------------------
BM_SVO_Set_With_RCU                       -0.0241         -0.0241           116           113           116           113
BM_SVO_Set_With_RCU                       -0.0310         -0.0309            87            84            87            84
BM_SVO_Set_With_RCU                       -0.0287         -0.0287            74            72            74            72
BM_SVO_Set_With_RCU                       -0.0249         -0.0248            74            72            74            72
BM_SVO_Set_With_RCU                       -0.0287         -0.0285            86            84            86            84
BM_SVO_Set_With_RCU                       -0.0290         -0.0289            74            72            74            72
BM_SVO_Set_With_RCU                       -0.0258         -0.0257            86            83            86            83
BM_SVO_Set_With_RCU                       -0.0285         -0.0284            86            83            86            83
BM_SVO_Set_With_RCU                       -0.0309         -0.0308            86            83            86            83
BM_SVO_Set_With_RCU                       -0.0303         -0.0303            74            72            74            72
BM_SVO_Set_With_RCU                       -0.0217         -0.0216            85            84            85            84
BM_SVO_Set_With_RCU                       -0.0318         -0.0245            87            84            86            84
BM_SVO_Set_With_RCU                       -0.0198         -0.0196            86            84            85            84
BM_SVO_Set_With_RCU                       -0.0321         -0.0313            86            83            86            83
BM_SVO_Set_With_RCU                       -0.0215         -0.0213            86            84            86            84
BM_SVO_Set_With_RCU                       -0.0222         -0.0221            86            84            86            84
BM_SVO_Set_With_RCU                       -0.0169         -0.0166            85            84            85            84
BM_SVO_Set_With_RCU                       -0.0297         -0.0295            74            72            74            72
BM_SVO_Set_With_RCU                       -0.0215         -0.0213            86            84            86            84
BM_SVO_Set_With_RCU                       -0.0211         -0.0207            85            84            85            84
BM_SVO_Set_With_RCU_pvalue                 0.0028          0.0028      U Test, Repetitions: 20 vs 20
BM_SVO_Set_With_RCU_mean                  -0.0259         -0.0254            84            82            84            82
BM_SVO_Set_With_RCU_median                -0.0230         -0.0227            86            84            86            84
BM_SVO_Set_With_RCU_stddev                -0.0153         -0.0144             9             9             9             9
BM_SVO_Set_With_RCU_cv                    +0.0109         +0.0113             0             0             0             0
OVERALL_GEOMEAN                           -0.0260         -0.0255             0             0             0             0
```


With Clone Check
```bash
 Performance counter stats for './build/experiment':

        78,668,880      branches                                                              
           261,346      branch-misses                    #    0.33% of all branches           

       0.061349001 seconds time elapsed

       0.047237000 seconds user
       0.014070000 seconds sys
```

With Clone Check
```bash
Peak Memory:
  Allocations : 566,417
  RCU Retire(): 12.6 MB
  SVO Set()   : 24.3 MB

==88322== HEAP SUMMARY:
==88322==     in use at exit: 0 bytes in 0 blocks
==88322==   total heap usage: 566,476 allocs, 566,476 frees, 62,165,594 bytes allocated
```

Without Clone Check
```bash
Performance counter stats for './build/experiment':

      132,699,451      branches                                                              
          552,776      branch-misses                    #    0.42% of all branches           

      0.114406032 seconds time elapsed

      0.065717000 seconds user
      0.046799000 seconds sys
```

Without Clone Check
```bash
Peak Memory:
  Allocations : 898,777
  RCU Retire(): 16.8 MB
  SVO Set()   : 71.9 MB

==88696== HEAP SUMMARY:
==88696==     in use at exit: 0 bytes in 0 blocks
==88696==   total heap usage: 898,837 allocs, 898,837 frees, 105,531,610 bytes allocated
```****