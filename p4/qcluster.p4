/* -*- P4_14 -*- */
#ifdef __TARGET_TOFINO__
#include <tofino/constants.p4>
#include <tofino/intrinsic_metadata.p4>
#include <tofino/primitives.p4>
//Include the blackbox definition
#include <tofino/stateful_alu_blackbox.p4>
#else
#warning This program is intended for Tofino P4 architecture only
#endif

#include "includes/header_and_parser.p4"

#define CM_LEN 65536
#define CM_HASH_LEN 16
#define INTERVAL 500000
//#define BF_LEN 65536
//#define BF_HASH_LEN 16
//#define ECN_THRESHOLD 10

header_type user_metadata_t {
    fields {
        egress_port_qid: 12;
        tstamp : 32;
        new_id : 48;
        count_1: 32;
        count_2: 32;
        count_3: 32;
        minCount: 20;
        queue_id: 3;
        enq_qdepth_sum: 20;
        qdepth_i: 32 (signed);
        //temp: 20 ;
        enq_qdepth: 19;
        is_small_packet : 1;
    }
}
metadata user_metadata_t md;

/******************************************************************************
 *
 * IPv4 routing
 *
 *****************************************************************************/

action hop(ttl, egress_port) {
    //add_to_field(ttl, -1);
    modify_field(ig_intr_md_for_tm.ucast_egress_port, egress_port);
}
action hop_ipv4(egress_port) {
    hop(ipv4.ttl, egress_port);
    //modify_field(ig_intr_md_for_tm.ucast_egress_port, egress_port);
    default_queue();
}
table ipv4_routing {
    reads {
        ipv4.dip : exact;
    }
    actions {
        hop_ipv4;
    }
    size: 512;
}

/******************************************************************************
 *
 * shift left
 *
 *****************************************************************************/

action shift_left_3_bit() {
    shift_left(md.egress_port_qid, ig_intr_md_for_tm.ucast_egress_port, 3);
}
table shift_left_3_bit_table {
    actions {shift_left_3_bit;}
    size : 1;
}

/******************************************************************************
 *
 * getTime
 *
 *****************************************************************************/
action getTime() {
    modify_field(md.tstamp, ig_intr_md_from_parser_aux.ingress_global_tstamp);
}
//@pragma stage 0
table getTimeTable {
    actions {getTime;}
    size: 1;
}

/******************************************************************************
 *
 * tstamp + counter
 *
 *****************************************************************************/
field_list flow_id_list{
    ig_intr_md_for_tm.ucast_egress_port;
    ipv4.proto;
    ipv4.sip;
    ipv4.dip;
    tcp.sPort;
    tcp.dPort;
}

field_list_calculation cm_hash_1 {
    input { flow_id_list; }
    algorithm: random;
    output_width: CM_HASH_LEN;
}
register tstamp_counter_1 {
    width : 64;
    instance_count: CM_LEN;
}
blackbox stateful_alu cm_alu_1 {
    reg: tstamp_counter_1;

    condition_lo: md.tstamp - register_hi > INTERVAL;
    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 1;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: register_lo + ipv4.totalLen;
    update_hi_1_value: md.tstamp;

    output_value : alu_lo;
    output_dst : md.count_1;
}
action check_cm_1() {
    cm_alu_1.execute_stateful_alu_from_hash(cm_hash_1);
}
//@pragma stage 1
table cm_1 {
    actions { check_cm_1; }
    size : 1;
}

field_list_calculation cm_hash_2 {
    input { flow_id_list; }
    algorithm: random;
    output_width: CM_HASH_LEN;
}
register tstamp_counter_2 {
    width : 64;
    instance_count: CM_LEN;
}
blackbox stateful_alu cm_alu_2 {
    reg: tstamp_counter_2;

    condition_lo: md.tstamp - register_hi > INTERVAL;
    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 1;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: register_lo + ipv4.totalLen;
    update_hi_1_value: md.tstamp;

    output_value : alu_lo;
    output_dst : md.count_2;
}
action check_cm_2() {
    cm_alu_2.execute_stateful_alu_from_hash(cm_hash_2);
}
//@pragma stage 1
table cm_2 {
    actions { check_cm_2; }
    size : 1;
}

field_list_calculation cm_hash_3 {
    input { flow_id_list; }
    algorithm: random;
    output_width: CM_HASH_LEN;
}
register tstamp_counter_3 {
    width : 64;
    instance_count: CM_LEN;
}
blackbox stateful_alu cm_alu_3 {
    reg: tstamp_counter_3;

    condition_lo: md.tstamp - register_hi > INTERVAL;
    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 1;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: register_lo + ipv4.totalLen;
    update_hi_1_value: md.tstamp;

    output_value : alu_lo;
    output_dst : md.count_3;
}
action check_cm_3() {
    cm_alu_3.execute_stateful_alu_from_hash(cm_hash_3);
}
table cm_3 {
    actions { check_cm_3; }
    size : 1;
}

action getMinCount1() {
    min(md.minCount, md.count_1, md.count_2);
}
table minCountTable1 {
    actions { getMinCount1; }
    size : 1;
}
action getMinCount2() {
    min(md.minCount, md.minCount, md.count_3);
}
table minCountTable2 {
    actions { getMinCount2; }
    size : 1;
}

/******************************************************************************
 *
 * queue_id
 *
 *****************************************************************************/
action get_queue_id(qid) {
    modify_field(md.queue_id, qid);
    modify_field(ig_intr_md_for_tm.qid, qid);
    add_to_field(md.egress_port_qid, qid);
}
action default_queue() {
    modify_field(md.queue_id, 7);
    modify_field(ig_intr_md_for_tm.qid, 7);
}
counter queueIdTable_stats {
    type : packets_and_bytes;
    direct : queueIdTable;
}
//@pragma stage 4
table queueIdTable {
    reads { 
        ig_intr_md_for_tm.ucast_egress_port: exact;
        md.minCount : range; 
    }
    actions {
        default_queue;
        get_queue_id;
    }
    default_action: default_queue();
    size : 64;
}

/******************************************************************************
 *
 * update pkt and weight
 *
 *****************************************************************************/
register pkt {
    width: 32;
    instance_count : 2048;
}
register weight {
    width: 64;
    instance_count : 2048;
}
blackbox stateful_alu pkt_alu {
    reg: pkt;
    update_lo_1_value: register_lo + 1;
}
blackbox stateful_alu weight_alu {
    reg: weight;
    condition_lo: register_lo + md.minCount >  2147483647;
    update_lo_1_predicate: not condition_lo;
    update_lo_1_value: register_lo + md.minCount;
    update_lo_2_predicate: condition_lo;
    update_lo_2_value: 0;
    update_hi_1_predicate: condition_lo;
    update_hi_1_value: register_hi + 1;
    update_hi_2_predicate: not condition_lo;
    update_hi_2_value: register_hi;
}
action pkt_incre() {
    pkt_alu.execute_stateful_alu(md.egress_port_qid);
}
action weight_incre() {
    weight_alu.execute_stateful_alu(md.egress_port_qid);
}
table pktTable {
    actions { pkt_incre; }
    size : 1;
}
table weightTable {
    actions { weight_incre; }
    size : 1;
}

/******************************************************************************
 *
 * ECN
 *
 *****************************************************************************/

action get_enq_qdepth() {
    modify_field(md.enq_qdepth, eg_intr_md.enq_qdepth);
}
table get_enq_qdepth_table {
    actions { get_enq_qdepth; }
    size : 1;
}

register qdepth_i {
    width: 64;
    instance_count : 8;
}
blackbox stateful_alu qdepth_i_alu {
    reg: qdepth_i;
    update_lo_1_value: md.enq_qdepth;
    output_dst: md.qdepth_i;
    output_value: register_lo;
}
action qdepth_i_action() {
    qdepth_i_alu.execute_stateful_alu(md.queue_id);
}
table qdepth_i_table {
    actions { qdepth_i_action; }
    size : 1;
}

action compute_qdepth_delta() {
    subtract(md.qdepth_i, md.enq_qdepth, md.qdepth_i);
}
table delta_table {
    actions { compute_qdepth_delta; }
    size : 1;
}

register qdepth_sum {
    width: 64;
    instance_count : 1;
}
blackbox stateful_alu qdepth_sum_alu {
    reg: qdepth_sum;
    update_lo_1_value: register_lo + md.qdepth_i;
    output_dst: md.enq_qdepth_sum;
    output_value: alu_lo;
}
action qdepth_sum_action() {
    qdepth_sum_alu.execute_stateful_alu();
}
table qdepth_sum_table {
    actions { qdepth_sum_action; }
    size : 1;
}

action mark_ecn() {
    modify_field(ipv4.ecn, 3);
}
action nop() {
}
table mark_ecn_table {
    reads {
        md.enq_qdepth_sum: range;
        //eg_intr_md.enq_qdepth: range;
    }
    actions {mark_ecn;}
    default_action: nop();
    size : 1;
}

/**
 * Add packet size table, if packet size if less than a threshold,
 * We will not process it.
 */
action set_small() {
    modify_field(md.is_small_packet, 1);
}

action unset_small() {
    modify_field(md.is_small_packet, 0);
}

table packet_size_table {
    reads { ipv4.totalLen : range; }
    actions { set_small; unset_small; }
    default_action : unset_small;
    size: 8;
}


/******************************************************************************
 *
 * Ingress
 *
 *****************************************************************************/

control ingress {
    if (valid(ipv4)) {
        apply(ipv4_routing);
        apply(packet_size_table);
        if (md.is_small_packet == 0) {
            apply(shift_left_3_bit_table);
            apply(getTimeTable);
            apply(cm_1);
            apply(cm_2);
            apply(cm_3);
            apply(minCountTable1);
            apply(minCountTable2);
            apply(queueIdTable);
            apply(weightTable);
            apply(pktTable);
        }
    }
}

control egress {
    apply(get_enq_qdepth_table);
    apply(qdepth_i_table);
    apply(delta_table);
    apply(qdepth_sum_table);    
    
    if ((ipv4.ecn == 1) or ipv4.ecn == 2) {
        apply(mark_ecn_table);
    }
}
