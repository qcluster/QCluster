/*
 * Weighted Round Robin
 *
 * Variables:
 * queue_num_: number of CoS queues
 * thresh_: ECN marking threshold
 * mean_pktsize_: configured mean packet size in bytes
 * marking_scheme_: Disable ECN (0), Per-queue ECN (1) and Per-port ECN (2)
 */

#include "FQ_Ideal.h"
#include "flags.h"
#include "math.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

static class FQ_IdealClass : public TclClass {
 public:
	FQ_IdealClass() : TclClass("Queue/FQ_Ideal") {}
	TclObject* create(int, const char*const*) {
		return (new FQ_Ideal);
	}
} class_priority;

void FQ_Ideal::enque(Packet* p)
{
	hdr_ip *iph = hdr_ip::access(p);
	hdr_flags* hf = hdr_flags::access(p);
	int qlimBytes = qlim_ * mean_pktsize_;
 
    queue_num_=max(min(queue_num_,MAX_QUEUE_NUM),1);
	char message_id[200];
	sprintf(message_id, "%d.%d.%d.%d@%d", iph->saddr(), iph->sport(), iph->daddr(), iph->dport(), iph->message_start());
	//queue length exceeds the queue limit
	if(TotalByteLength()+hdr_cmn::access(p)->size()>qlimBytes)
	{
		drop(p);
		return;
	}
	BOBHash32 hash_f(1);
	int m_id = hash_f.run(message_id, strlen(message_id)) % MAX_QUEUE_NUM;

	//Enqueue packet
	q_[m_id]->enque(p);
	
	
    //Enqueue ECN marking: Per-queue or Per-port
    if((marking_scheme_==PER_QUEUE_ECN && q_[m_id]->byteLength()>thresh_*mean_pktsize_)||
    (marking_scheme_==PER_PORT_ECN && TotalByteLength()>thresh_*mean_pktsize_))
    {
        if (hf->ect()) //If this packet is ECN-capable
            hf->ce()=1;
    }
}

Packet* FQ_Ideal::deque()
{
    if(TotalByteLength()>0)
	{
        //high->low: 0->7
		int rd = rand();
		Packet* ret_p;
		Qnode* qi_p = &queue_info_head;
		Qnode* qi = queue_info_head.next;
		for(int i=0;i<MAX_QUEUE_NUM;i++)
		{
			if(q_[qi->qid]->length()>0)
			{
				ret_p=q_[qi->qid]->deque();
				break;
			}
			qi_p = qi_p->next;
			qi = qi->next;
		}
		if(qi != queue_info_tail)
		{
			qi_p->next = qi->next;
			queue_info_tail->next = new Qnode;
			queue_info_tail->next->qid = qi->qid;
			queue_info_tail = queue_info_tail->next;
			delete qi;
		}
		return ret_p;
    }

	return NULL;
}
