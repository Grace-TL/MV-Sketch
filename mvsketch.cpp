#include "mvsketch.hpp"

MVSketch::MVSketch(int depth, int width, int lgn) {

    mv_.depth = depth;
    mv_.width = width;
    mv_.lgn = lgn;
    mv_.sum = 0;
    mv_.counts = new SBucket *[depth*width];
    for (int i = 0; i < depth*width; i++) {
        mv_.counts[i] = (SBucket*)calloc(1, sizeof(SBucket));
        memset(mv_.counts[i], 0, sizeof(SBucket));
        mv_.counts[i]->key[0] = '\0';
    }

    mv_.hash = new unsigned long[depth];
    mv_.scale = new unsigned long[depth];
    mv_.hardner = new unsigned long[depth];
    char name[] = "MVSketch";
    unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
    for (int i = 0; i < depth; i++) {
        mv_.hash[i] = GenHashSeed(seed++);
    }
    for (int i = 0; i < depth; i++) {
        mv_.scale[i] = GenHashSeed(seed++);
    }
    for (int i = 0; i < depth; i++) {
        mv_.hardner[i] = GenHashSeed(seed++);
    }
}

MVSketch::~MVSketch() {
    for (int i = 0; i < mv_.depth*mv_.width; i++) {
        free(mv_.counts[i]);
    }
    delete [] mv_.hash;
    delete [] mv_.scale;
    delete [] mv_.hardner;
    delete [] mv_.counts;
}


void MVSketch::Update(unsigned char* key, val_tp val) {
    mv_.sum += val;
    unsigned long bucket = 0;
    int keylen = mv_.lgn/8;
    for (int i = 0; i < mv_.depth; i++) {
        bucket = MurmurHash64A(key, keylen, mv_.hardner[i]) % mv_.width;
        int index = i*mv_.width+bucket;
        MVSketch::SBucket *sbucket = mv_.counts[index];
        sbucket->sum += val;
        if (sbucket->key[0] == '\0') {
            memcpy(sbucket->key, key, keylen);
            sbucket->count = val;
        } else if(memcmp(key, sbucket->key, keylen) == 0) {
            sbucket->count += val;
        } else {
            sbucket->count -= val;
            if (mv_likely(sbucket->count < 0)) {
                memcpy(sbucket->key, key, keylen);
                sbucket->count = -sbucket->count;
            }
        }
    }
}


void MVSketch::Query(val_tp thresh, std::vector<std::pair<key_tp, val_tp> >&results) {

    myset res;
    for (int i = 0; i < mv_.width*mv_.depth; i++) {
        if (mv_.counts[i]->sum > thresh) {
            key_tp reskey;
            memcpy(reskey.key, mv_.counts[i]->key, mv_.lgn/8);
            res.insert(reskey);
        }
    }

    for (auto it = res.begin(); it != res.end(); it++) {
        val_tp resval = 0;
        for (int j = 0; j < mv_.depth; j++) {
            unsigned long bucket = MurmurHash64A((*it).key, mv_.lgn/8, mv_.hardner[j]) % mv_.width;
            unsigned long index = j*mv_.width+bucket;
            val_tp tempval = 0;
            if (memcmp(mv_.counts[index]->key, (*it).key, mv_.lgn/8) == 0) {
                tempval = (mv_.counts[index]->sum - mv_.counts[index]->count)/2 + mv_.counts[index]->count;
            } else {
                tempval = (mv_.counts[index]->sum - mv_.counts[index]->count)/2;
            }
            if (j == 0) resval = tempval;
            else resval = std::min(tempval, resval);
        }
        if (resval > thresh ) {
            key_tp key;
            memcpy(key.key, (*it).key, mv_.lgn/8);
            std::pair<key_tp, val_tp> node;
            node.first = key;
            node.second = resval;
            results.push_back(node);
        }
    }
    std::cout << "results.size = " << results.size() << std::endl;
}



val_tp MVSketch::PointQuery(unsigned char* key) {
    return Up_estimate(key);
}

val_tp MVSketch::Low_estimate(unsigned char* key) {
    val_tp ret = 0;
    for (int i = 0; i < mv_.depth; i++) {
        unsigned long bucket = MurmurHash64A(key, mv_.lgn/8, mv_.hardner[i]) % mv_.width;
        unsigned long index = i*mv_.width+bucket;
        val_tp lowest = 0;
        if (memcmp(key, mv_.counts[index]->key, mv_.lgn/8) == 0) {
            lowest = mv_.counts[index]->count;
        } else lowest = 0;
        ret = std::max(ret, lowest);
    }
    return ret;
}

val_tp MVSketch::Up_estimate(unsigned char* key) {
    val_tp ret = 0;
    for (int i = 0; i < mv_.depth; i++) {
        unsigned long bucket = MurmurHash64A(key, mv_.lgn/8, mv_.hardner[i]) % mv_.width;
        unsigned long index = i*mv_.width+bucket;
        val_tp upest = 0;
        if (memcmp(key, mv_.counts[index]->key, mv_.lgn/8) == 0) {
            upest = (mv_.counts[index]->sum - mv_.counts[index]->count)/2 + mv_.counts[index]->count;
        } else upest = (mv_.counts[index]->sum - mv_.counts[index]->count)/2;
        if (i == 0) ret = upest;
        else ret = std::min(ret, upest);
    }
    return ret;
}





val_tp MVSketch::GetCount() {
    return mv_.sum;
}



void MVSketch::Reset() {
    mv_.sum=0;
    for (int i = 0; i < mv_.depth*mv_.width; i++){
        mv_.counts[i]->sum = 0;
        mv_.counts[i]->count = 0;
        memset(mv_.counts[i]->key, 0, mv_.lgn/8);
    }
}

void MVSketch::SetBucket(int row, int column, val_tp sum, long count, unsigned char* key) {
    int index = row * mv_.width + column;
    mv_.counts[index]->sum = sum;
    mv_.counts[index]->count = count;
    memcpy(mv_.counts[index]->key, key, mv_.lgn/8);
}

MVSketch::SBucket** MVSketch::GetTable() {
    return mv_.counts;
}

void MVSketch::MergeAll(MVSketch** mv_arr, int size) {
  long countarr[size];
  val_tp sumarr[size];
  unsigned char* keyarr[size];
  long est[size];

  for (int d = 0; d < mv_.depth; d++) {
    for (int w = 0; w < mv_.width; w++) {
      val_tp total = 0;
      for (int i = 0; i < size; i++) {
        //find the majority and its estimate
        MVSketch* cursk = mv_arr[i];
        MVSketch::SBucket** table = (cursk)->GetTable();
        int index = d*mv_.width+w;
        countarr[i] = table[index]->count;
        sumarr[i] = table[index]->sum;
        keyarr[i] = table[index]->key;
        total += sumarr[i];
      }

      int pointer = 0;
      long counttmp = 0;
      for (int i = 0; i  < size; i++) {
        est[i] = 0;
        for (int j = 0; j < size; j++) {
          if (memcmp(keyarr[i], keyarr[j], mv_.lgn/8) == 0) {
            est[i] += (sumarr[j] + countarr[j])/2;
          } else {
            est[i] += (sumarr[j] - countarr[j])/2;
          }
        }
        if (est[i] > est[pointer]) pointer = i;
      }
      counttmp = 2 * est[pointer] - total;
      counttmp = counttmp > 0 ? counttmp : 0;
      if (counttmp == 0) {
          SetBucket(d, w, 2*est[pointer], counttmp, keyarr[pointer]);
      } else {
          SetBucket(d, w, total, counttmp, keyarr[pointer]);
      }
    }
  }
}




