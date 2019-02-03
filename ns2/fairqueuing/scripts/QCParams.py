import os
import sys

sim_end = 100000
link_rate = 10
mean_link_delay = 0.0000002
host_delay = 0.00002
queueSize = 240 # large queue size to get stable simulation
load_arr = [0.9, 0.8, 0.7, 0.6, 0.5]
#load_arr = [0.9, 0.5]
connections_per_pair = 1

paretoShape = 1.05

enableMultiPath = 1
perflowMP = 0

sourceAlg = 'DCTCP-Sack'
#sourceAlg='LLDCT-Sack'
initWindow = 70
ackRatio = 1
slowstartrestart = 'true'
DCTCP_g = 0.0625
min_rto = 0.002
prob_cap_ = 5

switchAlg = 'FQ_QC'
DCTCP_K = 65
drop_prio_ = 'true'
prio_scheme_ = 2
deque_prio_ = 'true'
keep_order_ = 'true'
prio_num_arr = [8]
ECN_scheme_ = 2 #Per-port ECN marking

topology_spt = 16
topology_tors = 9
topology_spines = 4
topology_x = 1

ns_path = os.environ['HOME'] +\
    '/ns2/ns-allinone-2.34/ns-2.34/ns'
sim_script = 'spine_empirical.tcl'

workloadName = ['dctcp', 'vl2']
flow_cdf = ['CDF_DCTCP.tcl', 'CDF_vl2.tcl']
meanFlowSize = [1138*1460, 5117*1460]
