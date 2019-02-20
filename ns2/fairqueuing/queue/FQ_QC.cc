/*
 * Weighted Round Robin
 *
 * Variables:
 * queue_num_: number of CoS queues
 * thresh_: ECN marking threshold
 * mean_pktsize_: configured mean packet size in bytes
 * marking_scheme_: Disable ECN (0), Per-queue ECN (1) and Per-port ECN (2)
 */

#include "FQ_QC.h"
#include "flags.h"
#include "math.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

#define THRESHOLD 0.05

static class FQ_QCClass : public TclClass {
 public:
	FQ_QCClass() : TclClass("Queue/FQ_QC") {}
	TclObject* create(int, const char*const*) {
		return (new FQ_QC);
	}
} class_priority;

void FQ_QC::enque(Packet* p)
{
	hdr_ip *iph = hdr_ip::access(p);
	int qlimBytes = qlim_ * mean_pktsize_;
	int size = hdr_cmn::access(p)->size();
    // 1<=queue_num_<=MAX_QUEUE_NUM
    queue_num_=max(min(queue_num_,MAX_QUEUE_NUM),1);

	//queue length exceeds the queue limit
	if(TotalByteLength()+size>qlimBytes)
	{
		drop(p);
		return;
	}
	int queue_chose = 0;
	int flow_id = iph->flowid();
	int now_message = iph->message_start();
	
    if(flow_count.find(flow_id) != flow_count.end() && queue_num_ > 1 && message_id.find(flow_id) != message_id.end())
	{
	    if(message_id[flow_id] != now_message)
		{
			for(int i = 0 ; i < queue_num_ ; ++i)
			{
				if(count_flow_in[i].find(flow_id) != count_flow_in[i].end())
				{
					count_flow_in[i].erase(flow_id);
				}
			}
			flow_count[flow_id] = 0;
			goto ENQUE;
	    }
		int p_num = flow_count[flow_id];
		int Qthres[MAX_QUEUE_NUM];
		for(int i = 0 ; i < queue_num_-1 ; ++i)
		{
			int thre1 = 0;
			int thre2 = 0;
			if(pkt_num[i] > 0) thre1 += pkt_sum[i] / pkt_num[i];
			thre2 = thre1;
			if(pkt_num[i+1] > 0) thre2 += pkt_sum[i+1] / pkt_num[i+1];
			if(thre1 < 1) thre1 = 1;
			if(thre2 < 1) thre2 = 1;
			if(i > 0) 
			{
				thre1 += Qthres[i-1];
				thre2 += Qthres[i-1];
			}
			Qthres[i] = sqrt(double(thre1) * double(thre2));
		}

	    for(int i = 0 ;i < queue_num_ ; ++i)
		{	
			if(i == queue_num_ - 1 || p_num <= Qthres[i] + 1)
			{
		    	queue_chose = i;
				break;
		    }
		}
	}

ENQUE:
	//Enqueue packet
	if(current_flow[queue_chose].find(flow_id) == current_flow[queue_chose].end())
	{
		current_flow[queue_chose][flow_id] = 0;
	}
	current_flow[queue_chose][flow_id] += 1;

	q_[queue_chose]->enque(p);
	if(queue_num_ > 1)
	{
		if(flow_count.find(flow_id) == flow_count.end()) flow_count[flow_id] = 0;
		flow_count[flow_id] += 1;
		if(count_flow_in[queue_chose].find(flow_id) == count_flow_in[queue_chose].end())
		{
			count_flow_in[queue_chose].insert(flow_id);
		}
		pkt_num[queue_chose] += 1;
        pkt_num[queue_chose] += flow_count[flow_id];
		message_id[flow_id] = now_message;
	}
	hdr_flags* hf = hdr_flags::access(p);
    //Enqueue ECN marking: Per-queue or Per-port
    
    if((queue_chose == -1 && q_[queue_chose]->byteLength()>thresh_*mean_pktsize_)||
    (queue_chose >= 0 && TotalByteLength()>thresh_*mean_pktsize_))
    {
        if (hf->ect()) //If this packet is ECN-capable
            hf->ce()=1;
    }
}

Packet* FQ_QC::deque()
{
    if(TotalByteLength()>0)
	{
        //high->low: 0->7
	    int outq = 0;
		for(; outq < queue_num_; ++outq)
		{
			if(rr_times[outq] != 0) break;
		}
		if(outq == queue_num_)
		{
			outq = 0;
			int rd = rand() % 1000000;
			for(int i = 0 ; i < queue_num_; ++i)
			{
				rr_times[i] = current_flow[i].size();
			}
			for(; outq < queue_num_; ++outq)
			{
				if(rr_times[outq] != 0) break;
			}
		}
		rr_times[outq] -= 1;
		Packet* p = q_[outq]->deque();
		hdr_ip *iph = hdr_ip::access(p);
		int flow_id = iph->flowid();
		current_flow[outq][flow_id] -= 1;
		if(current_flow[outq][flow_id] == 0)
		{
			current_flow[outq].erase(flow_id);
		}
		return (p);
    }

	return NULL;
}


