#ifndef MVKETCH_H
#define MVKETCH_H
#include <vector>
#include <unordered_set>
#include <utility>
#include <cstring>
#include <cmath>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "datatypes.hpp"
extern "C"
{
#include "hash.h"
#include "util.h"
}



class MVSketch {

    typedef struct SBUCKET_type {
        //Total sum V(i, j)
        val_tp sum;

        long count;

        unsigned char key[LGN];

    } SBucket;

    struct MV_type {

        //Counter to count total degree
        val_tp sum;
        //Counter table
        SBucket **counts;

        //Outer sketch depth and width
        int depth;
        int width;

        //# key word bits
        int lgn;

        unsigned long *hash, *scale, *hardner;
    };


    public:
    MVSketch(int depth, int width, int lgn);

    ~MVSketch();

    void Update(unsigned char* key, val_tp value);

    val_tp PointQuery(unsigned char* key);

    void Query(val_tp thresh, myvector& results);

    val_tp Low_estimate(unsigned char* key);

    val_tp Up_estimate(unsigned char* key);

    val_tp GetCount();

    void Reset();

    private:

    MV_type mv_;
};

#endif
