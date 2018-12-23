#ifndef CHANGER_H
#define CHANGER_H

#include "mvsketch.hpp"
#include "datatypes.hpp"

template <class S>
class HeavyChanger {

public:
    HeavyChanger(int depth, int width, int lgn);

    ~HeavyChanger();

    void Update(unsigned char* key, val_tp val);

    void Query(val_tp thresh, myvector& result);

    void Reset();

    S* GetCurSketch();

    S* GetOldSketch();

private:
    S* old_sk;

    S* cur_sk;

    int lgn_;
};

template <class S>
HeavyChanger<S>::HeavyChanger(int depth, int width, int lgn) {
    old_sk = new S(depth, width, lgn);
    cur_sk = new S(depth, width, lgn);
    lgn_ = lgn;
}


template <class S>
HeavyChanger<S>::~HeavyChanger() {
    delete old_sk;
    delete cur_sk;
}

template <class S>
void HeavyChanger<S>::Update(unsigned char* key, val_tp val) {
    cur_sk->Update(key, val);
}

template <class S>
void HeavyChanger<S>::Query(val_tp thresh, myvector& result) {
    myvector res1, res2;
    cur_sk->Query(thresh, res1);
    old_sk->Query(thresh, res2);
    myset reset;
    for (auto it = res1.begin(); it != res1.end(); it++) {
        reset.insert(it->first);
    }
    for (auto it = res2.begin(); it != res2.end(); it++) {
        reset.insert(it->first);
    }
    val_tp old_low, old_up;
    val_tp new_low, new_up;
    val_tp diff1, diff2;
    val_tp change;
    for (auto it = reset.begin(); it != reset.end(); it++) {
        old_low = old_sk->Low_estimate((unsigned char*)(*it).key);
        old_up = old_sk->Up_estimate((unsigned char*)(*it).key);
        new_low = cur_sk->Low_estimate((unsigned char*)(*it).key);
        new_up = cur_sk->Up_estimate((unsigned char*)(*it).key);
        diff1 = new_up > old_low ? new_up - old_low : old_low - new_up;
        diff2 = new_low > old_up ? new_low - old_up : old_up - new_low;
        change = diff1 > diff2 ? diff1 : diff2;
        if (change > thresh) {
            key_tp key;
            memcpy(key.key, it->key, lgn_/8);
            std::pair<key_tp, val_tp> cand;
            cand.first = key;
            cand.second = change;
            result.push_back(cand);
        }
    }
}


template <class S>
void HeavyChanger<S>::Reset() {
    old_sk->Reset();
    S* temp = old_sk;
    old_sk = cur_sk;
    cur_sk = temp;
}

template <class S>
S* HeavyChanger<S>::GetCurSketch() {
    return cur_sk;
}

template <class S>
S* HeavyChanger<S>::GetOldSketch() {
    return old_sk;
}

#endif
