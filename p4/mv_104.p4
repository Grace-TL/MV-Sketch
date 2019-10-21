/* -* P4_16 -*- */
#include <core.p4>
#include <v1model.p4>

const bit<16> TYPE_IPV4 = 0x800;
const bit<8> IP_PROTOCOLS_TCP        =   6;
const bit<8> IP_PROTOCOLS_UDP        =  17;

/*************************************************************
*****************   HEADERS *********************************
**************************************************************/

typedef bit<9> egressSpec_t;
typedef bit<48> macAddr_t;
typedef bit<32> ip4Addr_t;

header ethernet_t {
    macAddr_t dstAddr;
    macAddr_t srcAddr;
    bit<16> etherType;
}


header ipv4_t {
    bit<4>  version;
    bit<4>  ihl;
    bit<8>  diffserv;
    bit<16> totalLen;
    bit<16> identification;
    bit<3>  flags;
    bit<13> fragOffset;
    bit<8>  ttl;
    bit<8>  protocol;
    bit<16> hdrChecksum;
    ip4Addr_t srcAddr;
    ip4Addr_t dstAddr;
}

header tcp_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<32> seqNo;
    bit<32> ackNo;
    bit<4> dataOffset;
    bit<4> res;
    bit<8> flags;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgentPtr;
}

header udp_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<16> length_;
    bit<16> checksum;
}

//define register arrays
//key fields
register<bit<64>>(2048) sketch_key1;
register<bit<64>>(2048) sketch_key2;

//sum fields
register<int<32>>(2048) sketch_sum;

//count fields
register<int<32>>(2048) sketch_count;

struct metadata {
    bit<32> hash;
    bit<64> flowkey1;
    bit<64> flowkey2;
    bit<64> tempkey1;
    bit<64> tempkey2;
    int<32> tempcount;
    int<32> tempsum;
    bit<1> repass;
    bit<1> flag;
}

struct headers {
    ethernet_t ethernet;
    ipv4_t ipv4;
    tcp_t tcp;
    udp_t udp;
}


/*********************************************************
***************** PARSER *********************************
**********************************************************/

parser MyParser(packet_in packet, 
	 	out headers hdr,
		inout metadata meta,
		inout standard_metadata_t standard_metadata) {

    state start {
	transition parse_ethernet;
    }

    state parse_ethernet {
	packet.extract(hdr.ethernet);
	transition select(hdr.ethernet.etherType) {
	    TYPE_IPV4: parse_ipv4;
	    default: accept;
	}
    }

    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        transition select(hdr.ipv4.fragOffset, hdr.ipv4.ihl,
                          hdr.ipv4.protocol) {
            (13w0x0, 4w0x5, IP_PROTOCOLS_TCP): parse_tcp;
            (13w0x0, 4w0x5, IP_PROTOCOLS_UDP): parse_udp;
            default: accept;
        }
    }

    state parse_tcp {
	packet.extract(hdr.tcp);
	transition accept;
    }

    state parse_udp {
	packet.extract(hdr.udp);
	transition accept;
    }


}


/*********************************************************
***************** CHECKSUM VERIFICATION *******************
**********************************************************/

control MyVerifyChecksum(inout headers hdr, inout metadata meta) {
    apply { }
}

/*********************************************************
***************** INGRESS PROCESSING *******************
**********************************************************/

control MyIngress(inout headers hdr, 
		  inout metadata meta,
		  inout standard_metadata_t standard_metadata) {
    action drop() {
	mark_to_drop();
    }

    //forward all packets to the specified port
    action set_egr(egressSpec_t port) {
	standard_metadata.egress_spec = port;
    }
 
    //action: calculate hash functions
    //store hash index of each packet in metadata
    action cal_hash() {
	hash(meta.hash, HashAlgorithm.crc32, 32w0, {meta.flowkey1, meta.flowkey2}, 11w2047);
    }


    action copy_key_tcp() {
	meta.flowkey1[31:0] = hdr.ipv4.srcAddr;
	meta.flowkey1[63:32] = hdr.ipv4.dstAddr;
	meta.flowkey2[15:0] = hdr.tcp.srcPort;
	meta.flowkey2[31:16] = hdr.tcp.dstPort;
	meta.flowkey2[39:32] = hdr.ipv4.protocol;
    }

    action copy_key_udp() {
	meta.flowkey1[31:0] = hdr.ipv4.srcAddr;
	meta.flowkey1[63:32] = hdr.ipv4.dstAddr;
	meta.flowkey2[15:0] = hdr.udp.srcPort;
	meta.flowkey2[31:16] = hdr.udp.dstPort;
	meta.flowkey2[39:32] = hdr.ipv4.protocol;
    }

    table get_flowkey_tcp {
	actions = {
 	    copy_key_tcp;      
  	}
	default_action = copy_key_tcp();

    }


    table get_flowkey_udp {
	actions = {
 	    copy_key_udp;      
  	}
	default_action = copy_key_udp();

    }

    table forward {
	key = {
	    standard_metadata.ingress_port: exact;
	}
	actions = {
	    set_egr;
	    drop;
	}
	size = 1024;
	default_action = drop();
    }

    //calculate hash values
    table hash_index {
	actions = {
 	    cal_hash;      
  	}
	default_action = cal_hash();
    }


    apply {
	if (hdr.ipv4.isValid()) {
	    //first pass
	    if (meta.repass == 0) {
		forward.apply(); 
		//construct flowkey information 
		if (hdr.ipv4.protocol == IP_PROTOCOLS_TCP) {
		    get_flowkey_tcp.apply();
		}
		if (hdr.ipv4.protocol == IP_PROTOCOLS_UDP) {
		    get_flowkey_udp.apply(); 
		}
		//calculate hash value
		hash_index.apply();

		//update sum
		sketch_sum.read(meta.tempsum, meta.hash);
		meta.tempsum = meta.tempsum + 1;
		sketch_sum.write(meta.hash, meta.tempsum);

		//compare candidate flow key with current flow key
		sketch_key1.read(meta.tempkey1, meta.hash);
		if (meta.tempkey1 != meta.flowkey1) {
		    meta.flag = 1; 
		}
		sketch_key2.read(meta.tempkey2, meta.hash);
		if (meta.tempkey2 != meta.flowkey2) {
		    meta.flag = 1; 
		}

		sketch_count.read(meta.tempcount, meta.hash);
		if (meta.flag == 1 && meta.tempcount == 0) {
		    meta.repass = 1;
		}
		if (meta.flag == 0) {
		    meta.tempcount = meta.tempcount + 1;
		} else if (meta.tempcount > 0){
		    meta.tempcount = meta.tempcount - 1;
		}
		sketch_count.write(meta.hash, meta.tempcount);

		if (meta.repass  == 1) {
		    resubmit({meta.repass, meta.flowkey1, meta.flowkey2, meta.hash});
		}

	    } else { //second pass
		sketch_key1.write(meta.hash, meta.flowkey1);
		sketch_key2.write(meta.hash, meta.flowkey2);
		sketch_count.read(meta.tempcount, meta.hash);
		meta.tempcount = 1 - meta.tempcount;
		sketch_count.write(meta.hash, meta.tempcount);
	    }
	}
    }
}



/*********************************************************
***************** EGRESS PROCESSING *******************
**********************************************************/

control MyEgress(inout headers hdr,
		 inout metadata meta,
		 inout standard_metadata_t standard_metadata) {
    apply { }
}

/*********************************************************
***************** CHECKSUM COMPUTATION *******************
**********************************************************/

control MyComputeChecksum(inout headers hdr, inout metadata meta) {
    apply{
	update_checksum(
	    hdr.ipv4.isValid(),
	    { hdr.ipv4.version,
	      hdr.ipv4.ihl,
	      hdr.ipv4.diffserv,
	      hdr.ipv4.totalLen,
	      hdr.ipv4.identification,
	      hdr.ipv4.flags,
	      hdr.ipv4.fragOffset,
	      hdr.ipv4.ttl,
	      hdr.ipv4.protocol,
	      hdr.ipv4.srcAddr,
	      hdr.ipv4.dstAddr},
	      hdr.ipv4.hdrChecksum,
	      HashAlgorithm.csum16);
    }
}


/*********************************************************
***************** DEPARSER *******************************
**********************************************************/

control MyDeparser(packet_out packet, in headers hdr) {
    apply {
	packet.emit(hdr.ethernet);
	packet.emit(hdr.ipv4);
        packet.emit(hdr.tcp);
        packet.emit(hdr.udp);
   
    }
}


/*********************************************************
***************** SWITCH *******************************
**********************************************************/

V1Switch(
MyParser(),
MyVerifyChecksum(),
MyIngress(),
MyEgress(),
MyComputeChecksum(),
MyDeparser()
) main;

