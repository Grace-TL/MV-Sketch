# MV-Sketch


---
### Paper
__Lu Tang, Qun Huang, and Patrick P. C. Lee.__
MV-Sketch: A Fast and Compact Invertible Sketch for Heavy Flow Detection in
Network Data Streams.
_INFOCOM 2019_

---
### Files
- mvsketch.hpp, mvsketch.cpp: the implementation of MV-Sketch
- mvsketch\_simd.hpp mvsketch\_simd.cpp: the simd version of MV-Sketch
- main\_hitter.cpp: example about heavy hitter detection
- main\_changer.cpp: example about heavy changer detection
- main\_simd.cpp: example about heavy hitter detection with SIMD optimized
  MV-Sketch

---
### Requirements

 To compile and run the examples, you need to 
 - install PcapPlusPlus
  packages from the [PcapPlusPlus Web-site](https://seladb.github.io/PcapPlusPlus-Doc/download.html).
 - specify the pcap files in "iptraces.txt"

