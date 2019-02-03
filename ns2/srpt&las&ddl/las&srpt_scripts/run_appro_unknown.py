import threading
import os
import Queue
import time 

#from piasSearchParams import *
#from piasHadoopParams import *
#from piasAllRpcParams import *
#from piasSearchRpcParams import *
#from piasKeyValueParams import *
from appro_unknownParams import *

def worker():
	while True:
		try:
			j = q.get(block = 0)
		except Queue.Empty:
			return
		#Make directory to save results
		os.system('mkdir -p '+j[1])
		os.system(j[0])

q = Queue.Queue()

for kind in range(len(workloadName)):
    for prio_num_ in prio_num_arr:
        for i in range(len(load_arr)):

            localtime = time.localtime()
            directory_name = 'appro_unknown_%s_%d_%lf_%.2d%.2d' % (
                workloadName[kind], int(load_arr[i]*100), inter_*1000,
                localtime.tm_hour,localtime.tm_min)

            #Simulation command
            cmd = ns_path+' '+sim_script+' '\
                +str(sim_end)+' '\
                +str(link_rate)+' '\
                +str(mean_link_delay)+' '\
                +str(host_delay)+' '\
                +str(queueSize)+' '\
                +str(load_arr[i])+' '\
                +str(connections_per_pair)+' '\
                +str(meanFlowSize[kind])+' '\
                +str(paretoShape)+' '\
                +str(flow_cdf[kind])+' '\
                +str(enableMultiPath)+' '\
                +str(perflowMP)+' '\
                +str(sourceAlg)+' '\
                +str(initWindow)+' '\
                +str(ackRatio)+' '\
                +str(slowstartrestart)+' '\
                +str(DCTCP_g)+' '\
                +str(min_rto)+' '\
                +str(prob_cap_)+' '\
                +str(switchAlg)+' '\
                +str(DCTCP_K)+' '\
                +str(drop_prio_)+' '\
                +str(prio_scheme_)+' '\
                +str(deque_prio_)+' '\
                +str(keep_order_)+' '\
                +str(prio_num_)+' '\
                +str(ECN_scheme_)+' '\
                +'0'+' '\
                +'0'+' '\
                +'0'+' '\
                +'0'+' '\
                +'0'+' '\
                +'0'+' '\
                +'0'+' '\
                +str(topology_spt)+' '\
                +str(topology_tors)+' '\
                +str(topology_spines)+' '\
                +str(topology_x)+' '\
                +str(interval_message[kind])+' '	\
                +str('./'+directory_name+'/flow.tr')+'  >'\
                +str('./'+directory_name+'/logFile.tr')
            print cmd
            q.put([cmd, directory_name])

#Create all worker threads
threads = []
number_worker_threads = 30

#Start threads to process jobs
for i in range(number_worker_threads):
	t = threading.Thread(target = worker)
	threads.append(t)
	t.start()

#Join all completed threads
for t in threads:
	t.join()
