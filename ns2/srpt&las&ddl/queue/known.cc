/*
 * Strict Priority Queueing (SP)
 *
 * Variables:
 * queue_num_: number of CoS queues
 * thresh_: ECN marking threshold
 * mean_pktsize_: configured mean packet size in bytes
 * marking_scheme_: Disable ECN (0), Per-queue ECN (1) and Per-port ECN (2)
 */

#include "known.h"
#include "flags.h"
#include "math.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

static class KnownClass : public TclClass {
 public:
	KnownClass() : TclClass("Queue/Known") {}
	TclObject* create(int, const char*const*) {
		return (new Known);
	}
} class_priority;

//#define INTERVAL 0.005

void Known::enque(Packet* p)
{
	hdr_ip *iph = hdr_ip::access(p);
//	int prio = iph->prio();
	int flow_id = iph->flowid();
	int size = hdr_cmn::access(p)->size();
	int prio = iph->prio();
	packet_t type = hdr_cmn::access(p)->ptype();
	hdr_flags* hf = hdr_flags::access(p);
	int qlimBytes = qlim_ * mean_pktsize_;
	int now = iph->connection();
    // 1<=queue_num_<=MAX_QUEUE_NUM
    queue_num_=max(min(queue_num_,MAX_QUEUE_NUM),1);

	//queue length exceeds the queue limit
	if(TotalByteLength() + size > qlimBytes)
	{
		drop(p);
		return;
	}

	int queue_chose = 0;

	if(type == PT_TCP && prio > 0){
	    double value = prio;

	    double thresholds[queue_num_ - 1] = {0};
    	    for (int i = 0; i < queue_num_ - 1; ++i) {
		if(info[i].distinct < 1){
		    if(i == 0)
		    	thresholds[i] = (1 << 25);
		    else 
			thresholds[i] = thresholds[i - 1] + (1 << 25);
		    continue;
		}
        	thresholds[i] = max(info[i].average(),
		sqrt(info[i].average()*info[i + 1].average()));
        	if (i > 0 && thresholds[i] <= thresholds[i - 1])
            	    thresholds[i] = thresholds[i - 1] + 1460;
    	    }

	    for (queue_chose = 0; queue_chose < queue_num_ - 1; ++queue_chose) {
        	if (value <= thresholds[queue_chose] + 1) break;
    	    }	    
	}

	//Enqueue packet
	q_[queue_chose]->enque(p);

	if(type == PT_TCP && prio > 0){
	   info[queue_chose].counting += prio;
           info[queue_chose].distinct += 1; 
	   if(Scheduler::instance().clock() - last_update > 1){
                last_update = Scheduler::instance().clock();
                Update();
           } 
	}

    //Enqueue ECN marking: Per-queue or Per-port
    if((marking_scheme_==PER_QUEUE_ECN && q_[queue_chose]->byteLength()>thresh_*mean_pktsize_)||
    (marking_scheme_==PER_PORT_ECN && TotalByteLength()>thresh_*mean_pktsize_))
    {
        if (hf->ect()) //If this packet is ECN-capable
            hf->ce()=1;
    }
}

Packet* Known::deque()
{
    if(TotalByteLength()>0)
	{
        //high->low: 0->7
	    for(int i=0;i<queue_num_;i++)
	    {
		    if(q_[i]->length()>0)
            {
			    Packet* p=q_[i]->deque();
		        return (p);
		    }
        }
    }

	return NULL;
}
