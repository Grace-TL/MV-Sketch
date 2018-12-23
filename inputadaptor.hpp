#ifndef INPUTADAPTOR_H
#define INPUTADAPTOR_H
#include <iostream>
#include <string.h>
#include "datatypes.hpp"

class InputAdaptor {


    typedef struct {
        unsigned char* databuffer;
        uint64_t cnt=0;
        uint64_t cur=0;
        unsigned char* ptr;
    } adaptor_t;

    public:
        InputAdaptor(std::string filename, uint64_t buffersize);

        ~InputAdaptor();

        int GetNext(tuple_t* t);

        void Reset();

        uint64_t GetDataSize();

    private:
        adaptor_t* data;

};
#endif
