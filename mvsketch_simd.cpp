#include "mvsketch_simd.hpp"
#include "nmmintrin.h"
#include "immintrin.h"

MVSketchSIMD::MVSketchSIMD(int depth, int width, int lgn) {

    ss_.depth = depth;
    ss_.width = width;
    ss_.lgn = lgn;
    ss_.sum = 0;
    ss_.counts = new SBucket *[depth*width];
    for (int i = 0; i < depth*width; i++) {
        ss_.counts[i] = (SBucket*)calloc(1, sizeof(SBucket));
        memset(ss_.counts[i], 0, sizeof(SBucket));
    }
    ss_.hash = new unsigned long[depth];
    ss_.scale = new unsigned long[depth];
    ss_.hardner = new unsigned long[depth];
    char name[] = "MVSketchSIMD";
    unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
    for (int i = 0; i < depth; i++) {
        ss_.hash[i] = GenHashSeed(seed++);
    }
    for (int i = 0; i < depth; i++) {
        ss_.scale[i] = GenHashSeed(seed++);
    }
    for (int i = 0; i < depth; i++) {
        ss_.hardner[i] = GenHashSeed(seed++);
    }
}

MVSketchSIMD::~MVSketchSIMD() {
    for (unsigned i = 0; i < ss_.depth*ss_.width; i++) {
        free(ss_.counts[i]);
    }
    delete [] ss_.hash;
    delete [] ss_.scale;
    delete [] ss_.hardner;
    delete [] ss_.counts;
}




void MVSketchSIMD::Update(unsigned char* key, val_tp val) {
    ss_.sum += val;
    int keylen = ss_.lgn/8;
    uint32_t onehash[4];
    MurmurHash3_x64_128( key, keylen, ss_.hardner[0], onehash);

    //union {__m256i vhash; uint64_t ohash[4];};
    //vhash =  _mm256_set_epi64x(onehash[3], onehash[2], onehash[1], onehash[0]);
    //__m256i vwidth = _mm256_set1_epi64x(ss_.width);
    //__m256i quotient = _mm256_div_epi64(vhash, vwidth);
    //__m256i vtemp =  _mm256_mul_epu32(vwidth, quotient);
    //__m256i vbucket = _mm256_sub_epi64(vhash, vtemp);
    //__m256i vrow = _mm256_set_epi64x(0, 1, 2, 3);
    //union {__m256i vindex; uint64_t index[4];};
    //vindex = _mm256_add_epi64(vbucket, _mm256_mul_epu32(vrow, vwidth));

    __m256i vhash = _mm256_set_epi32(0, onehash[0], 0, onehash[1], 0, onehash[2], 0, onehash[3]);
    __m256i vwidth = _mm256_set1_epi32(ss_.width);
    __m256i vbucket = _mm256_srli_epi64(_mm256_mul_epu32(vhash, vwidth), 32);
    __m256i vrow = _mm256_set_epi32(0, 0, 0, 1, 0, 2, 0, 3);
    union {__m256i vindex; unsigned long index[4];};
    vindex = _mm256_add_epi64(vbucket, _mm256_mul_epu32(vrow, vwidth));
    MVSketchSIMD::SBucket* sbucket[4] = {ss_.counts[index[0]], ss_.counts[index[1]], ss_.counts[index[2]], ss_.counts[index[3]]};
    sbucket[0]->sum += val;
    sbucket[1]->sum += val;
    sbucket[2]->sum += val;
    sbucket[3]->sum += val;

    __m256i vkey = _mm256_set_epi64x(*((uint64_t *)(sbucket[3]->key)), *((uint64_t *)(sbucket[2]->key)), *((uint64_t *)(sbucket[1]->key)), *((uint64_t *)(sbucket[0]->key)));
    __m256i comkey = _mm256_set1_epi64x(*((uint64_t *)key));
    union {__m256i vflag; uint64_t flag[4];};
    vflag = _mm256_cmpeq_epi64 (vkey, comkey);
    for (int i = 0; i < 4; i++) {
        if (flag[i] != 0) {
            sbucket[i]->count += val;
        } else {
            sbucket[i]->count -= val;
            if (sbucket[i]->count < 0) {
                memcpy_8(sbucket[i]->key, key);
                sbucket[i]->count = -sbucket[i]->count;
            }
        }
    }
}


void MVSketchSIMD::Query(val_tp thresh, std::vector<std::pair<key_tp, val_tp> >&results) {

    myset res;
    for (unsigned i = 0; i < ss_.width*ss_.depth; i++) {
        if (ss_.counts[i]->sum > thresh) {
            key_tp reskey;
            memcpy(reskey.key, ss_.counts[i]->key, ss_.lgn/8);
            res.insert(reskey);
        }
    }
    std::cout << "res.size = " << res.size() << std::endl;



    for (auto it = res.begin(); it != res.end(); it++) {
        val_tp resval = 0;

        uint32_t onehash[4];
        MurmurHash3_x64_128( (*it).key, ss_.lgn/8, ss_.hardner[0], onehash);
        __m256i vhash = _mm256_set_epi32(0, onehash[0], 0, onehash[1], 0, onehash[2], 0, onehash[3]);
        __m256i vwidth = _mm256_set1_epi32(ss_.width);
        __m256i vbucket = _mm256_srli_epi64(_mm256_mul_epu32(vhash, vwidth), 32);
        __m256i vrow = _mm256_set_epi32(0, 0, 0, 1, 0, 2, 0, 3);
        union {__m256i vindex; unsigned long index[4];};
        vindex = _mm256_add_epi64(vbucket, _mm256_mul_epu32(vrow, vwidth));

        for (unsigned j = 0; j < ss_.depth; j++) {
            val_tp tempval = 0;
            if (memcmp(ss_.counts[index[j]]->key, (*it).key, ss_.lgn/8) == 0) {
                tempval = (ss_.counts[index[j]]->sum - ss_.counts[index[j]]->count)/2 + ss_.counts[index[j]]->count;
            } else {
                tempval = (ss_.counts[index[j]]->sum - ss_.counts[index[j]]->count)/2;
            }
            if (j == 0) resval = tempval;
            else resval = std::min(tempval, resval);
        }
        if (resval > thresh ) {
            key_tp key;
            memcpy(key.key, (*it).key, ss_.lgn/8);
            std::pair<key_tp, val_tp> node;
            node.first = key;
            node.second = resval;
            results.push_back(node);
        }
    }
    std::cout << "results.size = " << results.size() << std::endl;
}



val_tp MVSketchSIMD::PointQuery(unsigned char* key) {
    return Up_estimate(key);
}

val_tp MVSketchSIMD::Low_estimate(unsigned char* key) {
    val_tp ret = 0;
    for (unsigned i = 0; i < ss_.depth; i++) {
        unsigned long bucket = MurmurHash64A(key, ss_.lgn/8, ss_.hardner[i]) % ss_.width;
        unsigned long index = i*ss_.width+bucket;
        val_tp lowest = 0;
        if (memcmp(key, ss_.counts[index]->key, ss_.lgn/8) == 0) {
            lowest = ss_.counts[index]->count;
        } else lowest = 0;
        ret = std::max(ret, lowest);
    }
    return ret;
}

val_tp MVSketchSIMD::Up_estimate(unsigned char* key) {
    val_tp ret = 0;
    for (unsigned i = 0; i < ss_.depth; i++) {
        unsigned long bucket = MurmurHash64A(key, ss_.lgn/8, ss_.hardner[i]) % ss_.width;
        unsigned long index = i*ss_.width+bucket;
        val_tp upest = 0;
        if (memcmp(key, ss_.counts[index]->key, ss_.lgn/8) == 0) {
            upest = (ss_.counts[index]->sum - ss_.counts[index]->count)/2 + ss_.counts[index]->count;
        } else upest = (ss_.counts[index]->sum - ss_.counts[index]->count)/2;
        if (i == 0) ret = upest;
        else ret = std::min(ret, upest);
    }
    return ret;
}





val_tp MVSketchSIMD::GetCount() {
    return ss_.sum;
}



void MVSketchSIMD::Reset() {
    ss_.sum=0;
    for (unsigned i = 0; i < ss_.depth*ss_.width; i++){
        ss_.counts[i]->sum = 0;
        ss_.counts[i]->count = 0;
        memset(ss_.counts[i]->key, 0, ss_.lgn/8);
    }
}

