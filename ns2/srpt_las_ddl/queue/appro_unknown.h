/*
 * Strict Priority Queueing (SP)
 *
 * Variables:
 * queue_num_: number of Class of Service (CoS) queues
 * thresh_: ECN marking threshold
 * mean_pktsize_: configured mean packet size in bytes
 * marking_scheme_: Disable ECN (0), Per-queue ECN (1) and Per-port ECN (2)
 */

#ifndef ns_priority_h
#define ns_priority_h

#define MAX_QUEUE_NUM 8

#define DISABLE_ECN 0
#define PER_QUEUE_ECN 1
#define PER_PORT_ECN 2

#include <string.h>
#include "queue.h"
#include "config.h"
#include <unordered_map>
#include "cm_sketch.h"

using namespace std;

struct Info{
    double counting;
    double distinct;

    double average(){
        if (distinct < 1)
            return 0;
        return counting / distinct;
    }
};

class Appro_Unknown : public Queue {
	public:
		Appro_Unknown()
		{
			queue_num_=MAX_QUEUE_NUM;
			thresh_=65;
			mean_pktsize_=1500;
			marking_scheme_=PER_PORT_ECN;
			last_update = 0;		
	
			//Bind variables
			bind("queue_num_", &queue_num_);
			bind("thresh_",&thresh_);
			bind("mean_pktsize_", &mean_pktsize_);
            		bind("marking_scheme_", &marking_scheme_);
			bind("interval_", &interval_);		
	
			sketch = new CM_Sketch(interval_);

			//Init queues
			q_=new PacketQueue*[MAX_QUEUE_NUM];
			for(int i=0;i<MAX_QUEUE_NUM;i++)
			{
				q_[i]=new PacketQueue;
				info[i].counting = 0;
                                info[i].distinct = 0;
			}
		}

		~Appro_Unknown()
		{
			delete sketch;
			for(int i=0;i<MAX_QUEUE_NUM;i++)
			{
				delete q_[i];
			}
			delete[] q_;
		}

	protected:
		void enque(Packet*);	// enqueue function
		Packet* deque();        // dequeue function

        PacketQueue **q_;		// underlying multi-FIFO (CoS) queues
		int mean_pktsize_;		// configured mean packet size in bytes
		int thresh_;			// single ECN marking threshold
		int queue_num_;			// number of CoS queues. No more than MAX_QUEUE_NUM
		int marking_scheme_;	// Disable ECN (0), Per-queue ECN (1) and Per-port ECN (2)
		double interval_;		

//		unordered_map<int, int> last_time;
  //              unordered_map<int, double> count_item;
		CM_Sketch* sketch;	
		double last_update;
		Info info[MAX_QUEUE_NUM];
		
		void Update(){
		   fprintf(stderr, "%lf\n", last_update);
                   for(int i = 0;i < queue_num_;++i){
                        if(info[i].distinct > 128){
                            info[i].counting /= 2.0;
                            info[i].distinct /= 2.0;
                        }
                        fprintf(stderr, "%d %lf %lf %lf\n",
                        i,info[i].average(),info[i].counting,info[i].distinct);  
//                      if(info[i].distinct < 1)
//                          info[i].counting = info[i].distinct = 0;
                   }

                   fprintf(stderr, "\n");
                }
			    

		//Return total queue length (bytes) of all the queues
		int TotalByteLength()
		{
			int bytelength=0;
			for(int i=0; i<MAX_QUEUE_NUM; i++)
				bytelength+=q_[i]->byteLength();
			return bytelength;
		}
};

#endif
