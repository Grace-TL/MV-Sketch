/* -* P4_16 -*- */
#include <core.p4>
#include <v1model.p4>

const bit<16> TYPE_IPV4 = 0x800;

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

//define register arrays
//key fields
register<bit<32>>(2048) sketch_key;

//sum fields
register<int<32>>(2048) sketch_sum;

//count fields
register<int<32>>(2048) sketch_count;

struct metadata {
    bit<32> hash;
    bit<32> tempkey;
    int<32> tempcount;
    int<32> tempsum;

}

struct headers {
    ethernet_t ethernet;
    ipv4_t ipv4;
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
	hash<bit<32>, bit<32>, tuple<bit<32>>, bit<32>>(meta.hash, HashAlgorithm.csum16, 32w0, {hdr.ipv4.srcAddr}, 32w2048);
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
	    hash_index.apply();
	    sketch_sum.read(meta.tempsum, meta.hash);
	    meta.tempsum = meta.tempsum + 1;
	    sketch_sum.write(meta.hash, meta.tempsum);

 	    sketch_key.read(meta.tempkey, meta.hash);
            sketch_count.read(meta.tempcount, meta.hash);
	    if (meta.tempkey != hdr.ipv4.srcAddr && meta.tempcount == 0) {
	        sketch_key.write(meta.hash, hdr.ipv4.srcAddr);
	    }
	    if (meta.tempkey == hdr.ipv4.srcAddr || meta.tempcount == 0) {
	   	meta.tempcount = meta.tempcount + 1; 
                sketch_count.write(meta.hash, meta.tempcount);
	    } else {
		meta.tempcount = meta.tempcount - 1; 
                sketch_count.write(meta.hash, meta.tempcount);
            }
            forward.apply(); 
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

