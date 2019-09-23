#ifndef CM_SKETCH_H
#define CM_SKETCH_H

#include "hash.h"

class CM_Sketch{
public:
    CM_Sketch(double interval, uint _HASH_NUM = 4, uint _LENGTH = 6401):
	interval_(interval), HASH_NUM(_HASH_NUM), LENGTH(_LENGTH){
	counter = new uint [LENGTH];
        arrive = new double [LENGTH];
        memset(counter, 0 ,sizeof(uint) * LENGTH);
        for(int i = 0;i < LENGTH;++i)
	    arrive[i] = -1;
    }
    ~CM_Sketch(){
        delete [] counter;
        delete [] arrive;
    }

    bool Init(uint flow_id, double now){
        bool ret = false;
	for(uint i = 0;i < HASH_NUM;++i){
            uint position = BOBHash32((uchar*)(&flow_id), sizeof(uint), i) % LENGTH;
            if(now - arrive[position] > interval_){
                counter[position] = 0;
		ret = true;
	    }

            counter[position] += 1460;
            arrive[position] = now;
        }
	return ret;
    }
    
    uint Query_count(uint flow_id){
        uint ret = 0x7fffffff;
        for(uint i = 0;i < HASH_NUM;++i){
            uint position = BOBHash32((uchar*)(&flow_id), sizeof(uint), i) % LENGTH;
            ret = MIN(ret, counter[position]);
        }
//	fprintf(stderr, "%d\n", ret);
        return ret;
    }

private:
    const uint HASH_NUM;
    const uint LENGTH;
    double interval_; 

    uint* counter;
    double* arrive;
};


#endif // CM_SKETCH_H

