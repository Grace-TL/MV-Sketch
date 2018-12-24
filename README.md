# MV-Sketch


---
### Paper
__Lu Tang, Qun Huang, and Patrick P. C. Lee.__
MV-Sketch: A Fast and Compact Invertible Sketch for Heavy Flow Detection in
Network Data Streams.
_INFOCOM 2019_

---
### Files
- mvsketch.hpp. mvsketch.cpp: the implementation of MV-Sketch
- mvsketch\_simd.hpp, mvsketch\_simd.cpp: the simd version of MV-Sketch
- main\_hitter.cpp: example about heavy hitter detection
- main\_changer.cpp: example about heavy changer detection
- main\_simd.cpp: example about heavy hitter detection with SIMD optimized
  MV-Sketch

---

### Compile and Run the examples
MV-Sketch is implemented with C++. We show how to compile the examples on
Ubuntu with g++ and make

#### Requirements
- Ensure __g++__ and __make__ are installed. 

- Ensure the necessary library PcapPlusPlus is installed.
    - You can refer to [PcapPlusPlus Web-site](https://seladb.github.io/PcapPlusPlus-Doc/download.html) to install the library.

- Specify the pcap files in "iptraces.txt". We provide two pcap files [here](https://drive.google.com/file/d/1BXilxUKTK18rZzRcfQIjBWnE1X8QF2B9/view?usp=sharing) for
  testing. One pcap file is regarded as one epoch in our examples. To run the 
  heavy changer example, you need to specify at least two pcap files.

#### Compile
- Compile examples with make

```
    $ make main_hitter
    $ make main_changer
    $ make main_simd
```
- To compile the SIMD example, you need to make sure your CPU and compiler can support AVX and AVX2.
    - Check your CPU with the command to make sure the flags contain "avx" and "avx2"
    ```
    $ cat /proc/cpuinfo
    ```
    - The g++ compiler should have version above 4.8   


#### Run
- Run the examples, and the program will output some statistics about the detection accuracy. 

```
$ ./main_hitter
$ ./main_changer
$ ./main_simd
```

- Note that you can change the configuration of MV-Sketch, e.g. number of rows and buckets in the example source code for testing.




