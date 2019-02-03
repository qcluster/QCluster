import importlib
import os
import math
import time
import datetime
# import atexit
import copy

from thrift.transport import TSocket, TTransport
from thrift.protocol import TBinaryProtocol, TMultiplexedProtocol

p4_name = 'qcluster'

import sys
sys.path.append(os.environ["SDE_INSTALL"] + "/lib/python2.7/site-packages/tofinopd/" + p4_name)
sys.path.append(os.environ["SDE_INSTALL"] + "/lib/python2.7/site-packages/tofino")
sys.path.append(os.environ["SDE_INSTALL"] + "/lib/python2.7/site-packages")

from res_pd_rpc.ttypes import *
from p4_pd_rpc.ttypes import *
from ptf.thriftutils import *

ip2port_conf = open(os.environ["P4_IP_TO_PORT_PATH"])
ip2port = [] # [("10.0.0.1", 20)]
for t in ip2port_conf:
    d = t.split()
    ip2port.append((d[0], int(d[1])))

def check_register_read(regs):
    not_zero = 0
    for reg in regs:
        if reg != 0:
            not_zero = not_zero + 1
    if not_zero <= 1:
        return True
    return False
def signToUnsign(signInt):
    if signInt < 0:
        return signInt + (1<<32)
    return signInt

class P4Connector(object):
    def __init__(self):
        p4_client_module = importlib.import_module("p4_pd_rpc." + p4_name)
        self.transport = TSocket.TSocket("localhost", 9090)
        self.bprotocol = TBinaryProtocol.TBinaryProtocol(self.transport)
        p4_protocol = TMultiplexedProtocol.TMultiplexedProtocol(self.bprotocol, p4_name)
        self.client = p4_client_module.Client(p4_protocol)
        conn_mgr_client_module = importlib.import_module('.'.join(['conn_mgr_pd_rpc', 'conn_mgr']))
        conn_mgr_protocol = TMultiplexedProtocol.TMultiplexedProtocol(self.bprotocol, 'conn_mgr')
        self.conn_mgr = conn_mgr_client_module.Client(conn_mgr_protocol)
        tm_client_module = importlib.import_module(".".join(["tm_api_rpc", "tm"]))
        tm_protocol = TMultiplexedProtocol.TMultiplexedProtocol(self.bprotocol, "tm")
        self.tm = tm_client_module.Client(tm_protocol)

        self.transport.open()
        print "Transport opened."

        self.sess_hdl = self.conn_mgr.client_init()
        self.dev_tgt = DevTarget_t(0, int(0xFFFF) - 0x10000)

    def set_property(self):
        self.client.mark_ecn_table_set_property(self.sess_hdl, self.dev_tgt.dev_id, 
            tbl_property_t.TBL_PROP_DUPLICATE_ENTRY_CHECK, 
            tbl_property_value_t.DUPLICATE_ENTRY_CHECK_DISABLE, 0)
        self.client.queueIdTable_set_property(self.sess_hdl, self.dev_tgt.dev_id, 
            tbl_property_t.TBL_PROP_DUPLICATE_ENTRY_CHECK, 
            tbl_property_value_t.DUPLICATE_ENTRY_CHECK_DISABLE, 0)
        print "Set property done."

    def set_routing(self):
        for kv in ip2port:
            match_spec = qcluster_ipv4_routing_match_spec_t(ipv4Addr_to_i32(kv[0]))
            action_spec = qcluster_hop_ipv4_action_spec_t(kv[1])
            try:
                self.client.ipv4_routing_table_delete_by_match_spec(self.sess_hdl, self.dev_tgt, match_spec)
            except:
                pass
            finally:
                self.client.ipv4_routing_table_add_with_hop_ipv4(self.sess_hdl, self.dev_tgt, match_spec, action_spec)

        print "Port add done."

    def set_ecn(self, start):
        start = int(start) * 1500 / 80
        end = (1 << 19) - 1 # qdepth has 19 bits, so its max val is (1 << 19) - 1
        match_spec = qcluster_mark_ecn_table_match_spec_t(start, end) 
        priority = 255

        while self.client.mark_ecn_table_get_entry_count(self.sess_hdl, self.dev_tgt) >= 1:
            entry = self.client.mark_ecn_table_get_first_entry_handle(self.sess_hdl, self.dev_tgt)
            self.client.mark_ecn_table_table_delete(self.sess_hdl, self.dev_tgt.dev_id, entry)
        self.client.mark_ecn_table_table_add_with_mark_ecn(self.sess_hdl, self.dev_tgt, match_spec, priority) 
        self.conn_mgr.complete_operations(self.sess_hdl)
        print "Add ECN rule done.", match_spec

    def set_small_packet(self):
        match_spec = qcluster_packet_size_table_match_spec_t(0, 100)
        priority = 0
        cnt = self.client.packet_size_table_get_entry_count(self.sess_hdl, self.dev_tgt)
        if cnt == 1:
            entry = self.client.packet_size_table_get_entry_count(self.sess_hdl, self.dev_tgt)
            self.client.packet_size_table_table_delete(self.sess_hdl, self.dev_tgt.dev_id, entry)
        self.client.packet_size_table_table_add_with_set_small(self.sess_hdl, self.dev_tgt, match_spec, priority)
        self.conn_mgr.complete_operations(self.sess_hdl)
        print "Set small packet done."

    def set_aging(self, interval=10):
        self.aging_interval = interval

    def set_interval_time(self, time=1.5):
        self.min_interval_time = time

    def set_qcluster_threholds(self, ports):
        reg_flag = qcluster_register_flags_t(read_hw_sync = True) # reg_flag for register_read.
        loop_time = 0
        queueIdTable_hdls_save = [[0 for i in range(8)] for j in range(len(ports))]

        # Set queues to good mode
        for port in ports:
            self.set_priority_mode(port)

        # initiate queueIdTable
        while self.client.queueIdTable_get_entry_count(self.sess_hdl, self.dev_tgt) != 0:
            hdl = self.client.queueIdTable_get_first_entry_handle(self.sess_hdl, self.dev_tgt)
            status = self.client.queueIdTable_table_delete(self.sess_hdl, self.dev_tgt.dev_id, hdl)
        
        #write some counters
        #ts = [0, 5, 10, 33, 70, 150, 320, 800]
        for port in ports:
            for i in range(0, 8):
                #self.client.register_write_weight(self.sess_hdl, self.dev_tgt, port * 8 + i, 10 * (ts[7 - i] + 2))
                self.client.register_write_weight(self.sess_hdl, self.dev_tgt, port * 8 + i, qcluster_weight_value_t(0, 0))
                #self.client.register_write_pkt(self.sess_hdl, self.dev_tgt, port * 8 + i, 10)
                self.client.register_write_pkt(self.sess_hdl, self.dev_tgt, port * 8 + i, 0)
        self.conn_mgr.complete_operations(self.sess_hdl)
        

        weight_rec = [[0 for j in range(8)] for i in range(len(ports))] # weight number read last time
        pkt_rec = [[0 for j in range(8)] for i in range(len(ports))]
        ts = [0, 5, 10, 33, 70, 150, 320, 800]
        #ts = [0, 1, 2, 3, 4, 6, 7, 8]
        #ts = [0, 0, 0, 0, 0, 0, 0, 0]
        #ts = [0, 200, 400, 600, 800, 1600, 3200, 6400]
        #ts = [0, 500, 1500, 4000, 10000, 32000, 80000, 655350]
        weight_counters = [[10*(ts[7-j]+2) for j in range(8)] for i in range(len(ports))] # weight number with aging
        #weight_counters = [[0 for j in range(8)] for i in range(len(ports))] # weight number with aging
        pkt_counters = [[10 for j in range(8)] for i in range(len(ports))]
        #pkt_counters = [[0 for j in range(8)] for i in range(len(ports))]
        weight_tmp = [0 for i in range(8)]  # tmp var
        pkt_tmp = [0 for i in range(8)] 
        # tmp = read_register()
        # inc_number = tmp - rec
        # rec = tmp
        # counters += inc_number
        # aging: counters = counters / 2

        # begin updating
        while True:
            print "====== loop time:", loop_time, "======="
            t1 = datetime.datetime.now()
            queueIdTable_ports_hdls = []

            port_i = 0
            for port in ports:
                if loop_time % self.aging_interval == 0: # aging
                    for i in range(8):
                        if pkt_counters[port_i][i] > 256:
                            pkt_counters[port_i][i] = pkt_counters[port_i][i] // 2
                            weight_counters[port_i][i] = weight_counters[port_i][i] // 2
                    print "aging..."

                # conn_mgr.begin_batch(sess_hdl)
                register_read_begin_t = datetime.datetime.now()
                #self.client.register_hw_sync_pkt(self.sess_hdl, self.dev_tgt)
                #self.client.register_hw_sync_weight(self.sess_hdl, self.dev_tgt)
                for i in range(0, 8):
                    t = self.client.register_read_pkt(self.sess_hdl, self.dev_tgt, port * 8 + i, reg_flag)
                    print "pkt", i, ":", t
                    pkt_tmp[i] = sum(t)
                    t = self.client.register_read_weight(self.sess_hdl, self.dev_tgt, port * 8 + i, reg_flag)
                    print "weight", i, ":", t
                    weight_tmp[i] = sum([((e.f0 << 31) + signToUnsign(e.f1)) for e in t])
                #range_t = self.client.register_range_read_weight(self.sess_hdl, self.dev_tgt, port * 8, 8, reg_flag)
                #print "weight range:", range_t
                print "read counters:", pkt_tmp
                print "read weights:", weight_tmp
                register_read_end_t = datetime.datetime.now()
                print "read register for port", port, "time:", (register_read_end_t - register_read_begin_t).total_seconds()
                
                print "Prev pkt:", pkt_rec[port_i]
                print "Prev weight:", weight_rec[port_i]
                print "This pkt", pkt_tmp
                print "This weight:", weight_tmp

                diff_avgs = []
                for i in range(0, 8):
                    pkt_diff =  pkt_tmp[i] - pkt_rec[port_i][i]
                    if pkt_diff == 0:
                        print "no more new packets?"
                    if pkt_diff < 0:
                        print "pkt overflow", i,":", pkt_tmp[i], pkt_rec[port_i][i], pkt_diff, 1 << 31
                        while pkt_diff < 0:
                            pkt_diff = pkt_diff + (1 << 31)
                    pkt_counters[port_i][i] = pkt_counters[port_i][i] + pkt_diff
                    pkt_rec[port_i][i] = pkt_tmp[i]
                    weight_diff = weight_tmp[i] - weight_rec[port_i][i]
                    if weight_diff < 0:
                        print "weight overflow",i,":", weight_tmp[i], weight_rec[port_i][i], weight_diff, 1 << 31
                        while weight_diff < 0:
                            weight_diff = weight_diff + (1 << 31)
                    weight_counters[port_i][i] = weight_counters[port_i][i] + weight_diff
                    weight_rec[port_i][i] = weight_tmp[i]
                    if pkt_diff != 0:
                        diff_avgs.append(weight_diff / pkt_diff)
                    else:
                        diff_avgs.append(0)
                print "Diff avgs: ", diff_avgs

                # Calculate averages and thresholds
                avgs = []
                for i in range(0, 8):
                    pkt_count = pkt_counters[port_i][i]
                    weight_count = weight_counters[port_i][i]
                    if pkt_count != 0:
                        avgs.append(float(weight_count) / float(pkt_count))
                    else:
                        avgs.append(1)
                    #print pkt_count, weight_count, avgs[-1]
                
                thresholds = []
                # Calculate thresholds. Notice that 7 is the highest priority
                avgs = list(reversed(avgs)) # value from small to large
                print "New avgs:", avgs
                for i in range(1, 8):
                    avgs[i] = max(avgs[i], avgs[i - 1] + 1)
                    thresholds.append(math.sqrt(avgs[i - 1] * avgs[i]))
                thresholds.append(1 << 18)


                # Write rules
                # TODO: use batch operations here?
                # conn_mgr.begin_batch(sess_hdl)
                table_write_begin_t = datetime.datetime.now()
                old_hdls = copy.copy(queueIdTable_hdls_save[port_i])
                self.conn_mgr.begin_batch(self.sess_hdl)
                new_hdls = []
                for i in range(0, 8):
                    start = 0
                    if i > 0:
                        start = int(math.floor(thresholds[i - 1])) + 1
                    end = int(math.floor(thresholds[i]))
                    match_spec = qcluster_queueIdTable_match_spec_t(port, start, end) # port, start, end
                    action_spec = qcluster_get_queue_id_action_spec_t(7 - i) # Flow in this range should be enqueued to i-th queue
                    priority = 255
                    #print "Rule add:", port, start, end,  7 - i
                    new_hdl = self.client.queueIdTable_table_add_with_get_queue_id(
                        self.sess_hdl, self.dev_tgt, match_spec, priority, action_spec)
                    new_hdls.append(new_hdl) 
                    #print match_spec

                # Remove old handles
                if loop_time != 0:
                    for hdl in queueIdTable_hdls_save[port_i]:
                        status = self.client.queueIdTable_table_delete(self.sess_hdl, self.dev_tgt.dev_id, hdl)
                queueIdTable_hdls_save[port_i] = new_hdls
                
                self.conn_mgr.end_batch(self.sess_hdl, True)
                self.conn_mgr.complete_operations(self.sess_hdl)

                # conn_mgr.end_batch(sess_hdl, False)
                table_write_end_t = datetime.datetime.now()
                print "write table for port", port, "time:", (table_write_end_t - table_write_begin_t).total_seconds()
                port_i = port_i + 1

            loop_time = loop_time + 1
            t2 = datetime.datetime.now()
            sec_pass =  (t2 - t1).total_seconds()
            if sec_pass < self.min_interval_time:
                time.sleep(self.min_interval_time - sec_pass)
                print "sec:",self.min_interval_time
            else:
                print "sec:",sec_pass

    def clear_all(self):
        # Clear registers
        self.client.register_reset_all_weight(self.sess_hdl, self.dev_tgt)
        self.client.register_reset_all_pkt(self.sess_hdl, self.dev_tgt)
        self.client.register_reset_all_tstamp_counter_1(self.sess_hdl, self.dev_tgt)
        self.client.register_reset_all_tstamp_counter_2(self.sess_hdl, self.dev_tgt)
        self.client.register_reset_all_tstamp_counter_3(self.sess_hdl, self.dev_tgt)
        self.client.register_reset_all_qdepth_i(self.sess_hdl, self.dev_tgt)

        # Clear queue id table
        while self.client.queueIdTable_get_entry_count(self.sess_hdl, self.dev_tgt) != 0:
            hdl = self.client.queueIdTable_get_first_entry_handle(self.sess_hdl, self.dev_tgt)
            status = self.client.queueIdTable_table_delete(self.sess_hdl, self.dev_tgt.dev_id, hdl)

        # Clear ecn table
        while self.client.mark_ecn_table_get_entry_count(self.sess_hdl, self.dev_tgt) != 0:
            entry = self.client.mark_ecn_table_get_first_entry_handle(self.sess_hdl, self.dev_tgt)
            self.client.mark_ecn_table_table_delete(self.sess_hdl, self.dev_tgt.dev_id, entry)

        self.conn_mgr.complete_operations(self.sess_hdl)
        print "Clear done."

    def set_priority_mode(self, port):
        for i in range(0, 8):
            self.tm.tm_set_q_sched_priority(0, port, i, i)
            self.tm.tm_set_q_remaining_bw_sched_priority(0, port, i, i)
        print "Set queue to priority mode done."

    def init_thresholds(self, ports):
        self.clear_all()
        ts = [0, 200, 65535 << 2]#, 80, 120, 1500, 3200, 8000, 655350]
        for port in ports:
            self.set_priority_mode(port)
            for i in range(0, 2):
                start = ts[i]
                end = ts[i + 1]
                qid = 6 - i
                match_spec = qcluster_queueIdTable_match_spec_t(port, start, end) # port, start, end
                action_spec = qcluster_get_queue_id_action_spec_t(qid) # Flow in this range should be enqueued to i-th queue
                priority = 255
                print "Rule add:", port, start, end,  qid
                self.client.queueIdTable_table_add_with_get_queue_id(self.sess_hdl, self.dev_tgt, match_spec, priority, action_spec)
        self.conn_mgr.complete_operations(self.sess_hdl)

    def print_reg(self):
        reg_flag = qcluster_register_flags_t(read_hw_sync = False) 
        for i in range(0, 8):
            t = self.client.register_read_qdepth_i(self.sess_hdl, self.dev_tgt, i, reg_flag)
            print t

    def close(self):
        self.conn_mgr.client_cleanup(self.sess_hdl)

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--ecn', type=int, help='integer, ECN threshold')
    parser.add_argument('--ports', type=str, help='comma separated integer ports')
    parser.add_argument('--init', default=False, action='store_true', help='whether to execute init_thresholds')
    parser.add_argument('--set', default=False, action='store_true', help='whether to execute set_qcluster_threholds')
    parser.add_argument('--clear', default=False, action='store_true', help='whether to execute clear_all')
    parser.add_argument('--print_reg', default=False, action='store_true', help='whether to execute print_reg')
    args = parser.parse_args()

    c = P4Connector()
    c.set_property()
    c.set_routing()
    c.set_small_packet()
    c.set_aging(1)
    c.set_interval_time(0)
    if (len(sys.argv) < 2):
        print "Command needed."
        exit()

    if args.ecn:
        assert args.ecn > 0, "Invalid arguments"
        c.set_ecn(args.ecn)

    if args.ports:
        ports = [int(x) for x in args.ports.split(",")]

    if args.set:
        assert args.ports is not None
        c.set_qcluster_threholds(ports)

    if args.init:
        assert args.ports is not None
        c.init_thresholds(ports)

    if args.clear:
        c.clear_all()

    if args.print_reg:
        c.print_reg()

    c.close()
