#include "heavy_changer.hpp"
#include "inputadaptor.hpp"
#include <unordered_map>
#include <utility>
#include "util.h"
#include "datatypes.hpp"
#include <iomanip>

typedef std::unordered_map<key_tp, val_tp*, key_tp_hash, key_tp_eq> groundmap;


int main(int argc, char* argv[]) {

    //Dataset list
    const char* filenames = "iptraces.txt";
    unsigned long long buf_size = 5000000000;
    //Heavy hitter threshold
    double thresh = 0.0008;

    //mv sketch parameters
    int mv_width = 5462; //number of buckets in each row
    int mv_depth = 4; //number of rows


    int numfile = 0;
    double precision=0, recall=0, error=0, throughput=0, detectime=0;
    double avpre=0, avrec=0, averr=0, avthr=0, avdet=0;

    std::string file;

    std::ifstream tracefiles(filenames);
    if (!tracefiles.is_open()) {
        std::cout << "Error opening file" << std::endl;
        return -1;
    }
    groundmap groundtmp;
    mymap ground;
    tuple_t t;
    memset(&t, 0, sizeof(tuple_t));
    val_tp diffsum = 0;

    //MVSketch
    HeavyChanger<MVSketch>* heavychangermv = new HeavyChanger<MVSketch>(mv_depth, mv_width, LGN*8);
    for (std::string file; getline(tracefiles, file);) {
        //load traces and get ground

        InputAdaptor* adaptor =  new InputAdaptor(file, buf_size);
        std::cout << "[Dataset]: " << file << std::endl;
        memset(&t, 0, sizeof(tuple_t));

        //Get ground
        adaptor->Reset();

        //Reset gounrdtmp;
        for (auto it = groundtmp.begin(); it != groundtmp.end(); it++) {
            it->second[1] = it->second[0];
            it->second[0] = 0;
        }

        //insert hash table
        while(adaptor->GetNext(&t) == 1) {
            key_tp key;
            memcpy(key.key, &(t.key), LGN);
            if (groundtmp.find(key) != groundtmp.end()) {
                groundtmp[key][0] += t.size;
            } else {
                val_tp* valtuple = new val_tp[2]();
                groundtmp[key] = valtuple;
                groundtmp[key][0] += t.size;
            }
        }

        if (numfile != 0) {
            ground.clear();
            val_tp oldval, newval, diff;
            diffsum = 0;
            //Get sum of change
            for (auto it = groundtmp.begin(); it != groundtmp.end(); it++) {
                oldval = it->second[0];
                newval = it->second[1];
                diff = newval > oldval ? newval - oldval : oldval - newval;
                diffsum += diff;
            }
            //Get heavy changer
            for (auto it = groundtmp.begin(); it != groundtmp.end(); it++) {
                oldval = it->second[0];
                newval = it->second[1];
                diff = newval > oldval ? newval - oldval : oldval - newval;
                if (diff > thresh*(diffsum)){
                    ground[it->first] = diff;
                }
            }
        }


        //Update sketch
        uint64_t t1, t2;
        //Update mv
        adaptor->Reset();
        t1 = now_us();
        heavychangermv->Reset();
        while(adaptor->GetNext(&t) == 1) {
            heavychangermv->Update((unsigned char*)&(t.key), (val_tp)t.size);
        }
        t2 = now_us();
        throughput = adaptor->GetDataSize()/(double)(t2-t1)*1000000;
        if (numfile != 0) {
            std::vector<std::pair<key_tp, val_tp> > results;
            //Query
            results.clear();
            val_tp threshold = thresh*diffsum;
            t1 = now_us();
            heavychangermv->Query(threshold, results);
            t2 = now_us();
            detectime = (double)(t2-t1)/1000000;
            //Evaluate accuracy

            int tp = 0, cnt = 0;;
            error = 0;
            for (auto it = ground.begin(); it != ground.end(); it++) {
                if (it->second > threshold) {
                    cnt++;
                    for (auto res = results.begin(); res != results.end(); res++) {
                        if (memcmp(it->first.key, res->first.key, sizeof(res->first.key)) == 0) {
                            error += abs(res->second - it->second)*1.0/it->second;
                            tp++;
                        }
                    }
                }
            }
            precision = tp*1.0/results.size();
            recall = tp*1.0/cnt;
            error = error/tp;
            avthr += throughput;
        }
        avpre += precision; avrec += recall; averr += error; avdet += detectime;
        numfile++;
        delete adaptor;
    }

    //Delete
    for (auto it = groundtmp.begin(); it != groundtmp.end(); it++) {
        delete [] it->second;
    }
    delete heavychangermv;


    for (int i = 1; i < numfile; i++) {
        std::cout << "----------------------------------------------" << std::endl;
        //Output to standard output
        std::cout << std::setfill(' ');
        std::cout << std::setw(20) << std::left << "Sketch" << std::setw(20) << std::left << "Precision"
            << std::setw(20) << std::left << "Recall" << std::setw(20)
            << std::left << "Relative Error" << std::setw(20) << std::left << "Throughput" << std::setw(20)
            << std::left << "Detection Time" << std::endl;
        std::cout << std::setw(20) <<  "MVSketch";
        std::cout << std::setw(20) << std::left << precision << std::setw(20) << std::left << recall << std::setw(20)
            << std::left << error << std::setw(20) << std::left << throughput << std::setw(20)
            << std::left << detectime << std::endl;
    }
    std::cout << "-----------------------------------------------   Summary    -------------------------------------------------------" << std::endl;
        //Output to standard output
        std::cout << std::setfill(' ');
        std::cout << std::setw(20) << std::left << "Sketch"
            << std::setw(20) << std::left << "Precision"
            << std::setw(20) << std::left << "Recall" << std::setw(20)
            << std::left << "Relative Error" << std::setw(20) << std::left << "Throughput" << std::setw(20)
            << std::left << "Detection Time" << std::endl;
            std::cout << std::setw(20) <<  "MVSketch";
            std::cout  << std::setw(20) << std::left << avpre/(numfile-1) << std::setw(20) << std::left << avrec/(numfile-1) << std::setw(20)
                << std::left << averr/(numfile-1) << std::setw(20) << std::left << avthr/(numfile-1) << std::setw(20)
                << std::left << avdet/(numfile-1) << std::endl;
}
