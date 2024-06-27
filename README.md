# ADMS'24 submission AE

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

<!--
## Further Support
If you have any enquiries, please contact huang@comp.nus.edu.sg (Huang Wentao) for the further support.

## Bibliography
If you use this work/code, please kindly cite our paper as follows:
```
```
-->
