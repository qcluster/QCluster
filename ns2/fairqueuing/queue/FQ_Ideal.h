/*
 * Weighted Round Robin
 *
 * Variables:
 * queue_num_: number of Class of Service (CoS) queues
 * thresh_: ECN marking threshold
 * mean_pktsize_: configured mean packet size in bytes
 * marking_scheme_: Disable ECN (0), Per-queue ECN (1) and Per-port ECN (2)
 */

#ifndef ns_FQ_Ideal_h
#define ns_FQ_Ideal_h

#define MAX_QUEUE_NUM 100

#define DISABLE_ECN 0
#define PER_QUEUE_ECN 1
#define PER_PORT_ECN 2

#include <string.h>
#include "queue.h"
#include "config.h"
#include <sstream>
#include "BOBHash32.h"
#include <stdlib.h>

class FQ_Ideal : public Queue {
	public:
		FQ_Ideal()
		{
			queue_num_=MAX_QUEUE_NUM;
			thresh_=65;
			mean_pktsize_=1500;
			marking_scheme_=PER_PORT_ECN;

			//Bind variables
			bind("queue_num_", &queue_num_);
			bind("thresh_",&thresh_);
			bind("mean_pktsize_", &mean_pktsize_);
            bind("marking_scheme_", &marking_scheme_);

			//Init queues
			q_=new PacketQueue*[MAX_QUEUE_NUM];
			Qnode* qi = &queue_info_head;
			for(int i=0;i<MAX_QUEUE_NUM;i++)
			{
				q_[i]=new PacketQueue;
				qi->next = new Qnode;
				qi = qi->next;
				qi->qid = i;
			}
			queue_info_tail = qi;
		}

		~FQ_Ideal()
		{
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
		struct Qnode{
			  int qid;
			  Qnode* next;
		};
		Qnode queue_info_head;
		Qnode* queue_info_tail;

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
