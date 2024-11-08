# Bandwidth Expansion via CXL: A Pathway to Accelerating In-Memory Analytical Processing.
Code repository for our [ADMS'24 paper](https://vldb.org/workshops/2024/proceedings/ADMS/ADMS24_01.pdf). 

## Prerequisites

- A two-socket machine with 4th generation Intel Xeon scalable processors.  
- CXL type3 memory device.
- [mm: mempolicy: N:M interleave policy for tiered memory nodes](https://lore.kernel.org/linux-mm/YqD0%2FtzFwXvJ1gK6@cmpxchg.org/T/)


## Microbenchmark
```
cd scripts/microbenchmark
bash bandwidth.sh
bash join.sh
bash skip.sh
bash part.sh
```

## SSB
Please refer to [crystal](https://github.com/anilshanbhag/crystal/) for SSB dataset generation.

```
cd scripts/ssb/
bash ssb.sh
```


## Further Support
If you have any enquiries, please contact huang@comp.nus.edu.sg (Huang Wentao) for the further support.

## Bibliography
If you use this work/code, please kindly cite the following paper:
```
@inproceedings{DBLP:conf/vldb/Huang0LCHT24,
  author       = {Wentao Huang and
                  Mo Sha and
                  Mian Lu and
                  Yuqiang Chen and
                  Bingsheng He and
                  Kian{-}Lee Tan},
  title        = {Bandwidth Expansion via {CXL:} {A} Pathway to Accelerating In-Memory
                  Analytical Processing},
  booktitle    = {Proceedings of Workshops at the 50th International Conference on Very
                  Large Data Bases, {VLDB} 2024, Guangzhou, China, August 26-30, 2024},
  publisher    = {VLDB.org},
  year         = {2024},
  url          = {https://vldb.org/workshops/2024/proceedings/ADMS/ADMS24\_01.pdf},
  timestamp    = {Thu, 07 Nov 2024 15:01:26 +0100},
  biburl       = {https://dblp.org/rec/conf/vldb/Huang0LCHT24.bib},
  bibsource    = {dblp computer science bibliography, https://dblp.org}
}
```
