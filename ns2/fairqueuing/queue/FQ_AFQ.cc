/*
 * Strict Priority Queueing (SP)
 *
 * Variables:
 * queue_num_: number of CoS queues
 * thresh_: ECN marking threshold
 * mean_pktsize_: configured mean packet size in bytes
 * marking_scheme_: Disable ECN (0), Per-queue ECN (1) and Per-port ECN (2)
 */

#include "FQ_AFQ.h"
#include "flags.h"
#include "math.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

#define THRESHOLD 0.05
#define BpR 100

static class FQ_AFQClass : public TclClass {
 public:
	FQ_AFQClass() : TclClass("Queue/FQ_AFQ") {}
	TclObject* create(int, const char*const*) {
		return (new FQ_AFQ);
	}
} class_priority;

void FQ_AFQ::enque(Packet* p)
{
	hdr_ip *iph = hdr_ip::access(p);
	hdr_flags* hf = hdr_flags::access(p);
	int qlimBytes = qlim_ * mean_pktsize_;
    // 1<=queue_num_<=MAX_QUEUE_NUM
    queue_num_=max(min(queue_num_,MAX_QUEUE_NUM),1);
    
	//queue length exceeds the queue limit
	if(TotalByteLength()+hdr_cmn::access(p)->size() > qlimBytes)
	{
		drop(p);
		return;
	}
    int flow_id = iph->flowid();
    int now_message = iph->message_start();
    
    if(flow_count.find(flow_id) != flow_count.end() && queue_num_ > 1 && message_id.find(flow_id) != message_id.end())
    {
        if(message_id[flow_id] != now_message)
        {
            flow_count[flow_id] = 0;
        }
    }
    if(flow_count.find(flow_id) == flow_count.end()) flow_count[flow_id] = 0;
	message_id[flow_id] = now_message;

	int bid = flow_count[flow_id];
	bid += R*BpR;
	bid += 1;
	int queue_chose = (int(bid / BpR)) % 8;
	if(int(bid / BpR) > 15)
	{
		queue_chose = (R + 7) % 8;
	}
	flow_count[flow_id] += 1;
	//Enqueue packet
	q_[queue_chose]->enque(p);
	
    //Enqueue ECN marking: Per-queue or Per-port
    if((marking_scheme_ == PER_QUEUE_ECN && q_[queue_chose]->byteLength() > thresh_*mean_pktsize_)||
    (marking_scheme_ == PER_PORT_ECN && TotalByteLength() > thresh_*mean_pktsize_))
    {
        if (hf->ect()) //If this packet is ECN-capable
            hf->ce()=1;
    }
}

Packet* FQ_AFQ::deque()
{
    if(TotalByteLength() > 0)
	{
        //high->low: 0->7
		for(int i = 0; i < MAX_QUEUE_NUM ; ++i)
		{
			if(q_[R % MAX_QUEUE_NUM]->length() == 0)
				R = (R+1) % MAX_QUEUE_NUM;
		}
		Packet* ret_p = q_[R]->deque();
		return ret_p;
	}  
	return NULL;
}

