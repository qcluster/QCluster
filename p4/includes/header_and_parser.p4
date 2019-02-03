
header_type ethernet_t {
    fields {
        dmac : 48;
        smac : 48;
        ethertype : 16;
    }
}
header ethernet_t ethernet;

header_type vlan_tag_t {
    fields {
        pri     : 3;
        cfi     : 1;
        vlan_id : 12;
        etherType : 16;
    }
}
header vlan_tag_t vlan_tag;

header_type ipv4_t {
    fields {
        ver : 4;
        len : 4;
        diffserv : 6;
        ecn: 2;
        totalLen : 16;
        id : 16;
        flags : 3;
        offset : 13;
        ttl : 8;
        proto : 8;
        csum : 16;
        sip : 32;
        dip : 32;
    }
}
header ipv4_t ipv4;


field_list ipv4_checksum_list {
        ipv4.ver;
        ipv4.len;
        ipv4.diffserv;
        ipv4.ecn;
        ipv4.totalLen;
        ipv4.id;
        ipv4.flags;
        ipv4.offset;
        ipv4.ttl;
        ipv4.proto;
        ipv4.sip;
        ipv4.dip;
}

field_list_calculation ipv4_checksum {
    input {
        ipv4_checksum_list;
    }
    algorithm : csum16;
    output_width : 16;
}

calculated_field ipv4.csum  {
    update ipv4_checksum;
}


header_type tcp_t {
    fields {
        sPort : 16;
        dPort : 16;
        seqNo : 32;
        ackNo : 32;
        dataOffset : 4;
        res : 3;
        ecn : 3;
        ctrl : 6;
        window : 16;
        checksum : 16;
        urgentPtr : 16;
    }
}
header tcp_t tcp;

header_type udp_t {
    fields {
        sPort : 16;
        dPort : 16;
        hdr_length : 16;
        checksum : 16;
    }
}
header udp_t udp;

parser start {
    return parse_ethernet;
}

parser parse_ethernet {
    extract(ethernet);
    return select(latest.ethertype) {
        0x0800 : parse_ipv4;
        0x8100 : parse_vlan_tag;
        default : ingress;
    }
}

parser parse_vlan_tag {
    extract(vlan_tag);
    return select(latest.etherType) {
        0x800 : parse_ipv4;
        default : ingress;
    }
}

parser parse_ipv4 {
    extract(ipv4);
    return select(latest.proto) {
        6 : parse_tcp;
        17: parse_udp;
        default: ingress;
    }
}
parser parse_tcp {
    extract(tcp);
    return ingress;
}
parser parse_udp {
    extract(udp);
    return ingress;
}
