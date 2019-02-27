#include "adaptor.hpp"
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <pcap.h>

Adaptor::Adaptor(std::string filename, uint64_t buffersize) {
    unsigned long test = 0;
    data = (adaptor_t*)calloc(1, sizeof(adaptor_t));
    data->databuffer = (unsigned char*)calloc(buffersize, sizeof(unsigned char));
    data->ptr = data->databuffer;
    data->cnt = 0;
    data->cur = 0;
    //Read pcap file
    std::string path = filename;
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* pfile = pcap_open_offline(filename.c_str(), errbuf);
    if (pfile == NULL) {
        std::cout << "[Error] Fail to open pcap file" << std::endl;
        exit(-1);
    }
    unsigned char* p = data->databuffer;
    //pcpp::RawPacket rawPacket;
    const u_char* rawpkt; // raw packet
    struct pcap_pkthdr hdr;

    struct ip* ip_hdr;
    while ((rawpkt = pcap_next(pfile, &hdr)) != NULL) {
        int status = 1;
        int eth_len = ETH_LEN;
        // error checking (Ethernet level)
        if (eth_len == 14) {
            struct ether_header* eth_hdr = (struct ether_header*)rawpkt;
            if (ntohs(eth_hdr->ether_type) == ETHERTYPE_VLAN) {
                eth_len = 18;
            }
            else if (ntohs(eth_hdr->ether_type) != ETH_P_IP) {
                status = -1;
            }
        }
        else if (eth_len == 4) {
            if (ntohs(*(uint16_t*)(rawpkt + 2)) != ETH_P_IP) {
                status = -1;
            }
        }
        else if (eth_len != 0) {
            // unkown ethernet header length
            status = -1;
        }
        int pkt_len = (hdr.caplen < MAX_CAPLEN) ? hdr.caplen : MAX_CAPLEN;
        uint32_t len = pkt_len - eth_len;
        // error checking (IP level)
        ip_hdr = (struct ip*)(rawpkt + eth_len);
        // i) IP header length check
        if ((int)len < (ip_hdr->ip_hl << 2)) {
            status = -1;
        }
        // ii) IP version check
        if (ip_hdr->ip_v != 4) {
            status = -1;
        }
        // iii) IP checksum check
        if (IP_CHECK && in_chksum_ip((unsigned short*)ip_hdr, ip_hdr->ip_hl << 2)) {
            status = -1;
        }

        // error checking (TCP/UDP/ICMP layer test)
        struct tcphdr* tcp_hdr;
        if (ip_hdr->ip_p == IPPROTO_TCP) {
            // see if the TCP header is fully captured
            tcp_hdr = (struct tcphdr*)((uint8_t*)ip_hdr + (ip_hdr->ip_hl << 2));
            if ((int)len < (ip_hdr->ip_hl << 2) + (tcp_hdr->doff << 2)) {
                status = -1;
            }
        } else if (ip_hdr->ip_p == IPPROTO_UDP) {
            // see if the UDP header is fully captured
            if ((int)len < (ip_hdr->ip_hl << 2) + 8) {
                status = -1;
            }
        } else if (ip_hdr->ip_p == IPPROTO_ICMP) {
            // see if the ICMP header is fully captured
            if ((int)len < (ip_hdr->ip_hl << 2) + 8) {
                status = -1;
            }
        }

        if (status == 1) {
            // assign the fields
            uint16_t srcport, dstport, iplen;
            iplen = ntohs(ip_hdr->ip_len);
            if (ip_hdr->ip_p == IPPROTO_TCP) {
                // TCP
                tcp_hdr = (struct tcphdr*)((uint8_t*)ip_hdr + (ip_hdr->ip_hl << 2));
                srcport = ntohs(tcp_hdr->source);
                dstport = ntohs(tcp_hdr->dest);
            }
            else if (ip_hdr->ip_p == IPPROTO_UDP) {
                // UDP
                struct udphdr* udp_hdr = (struct udphdr*)((uint8_t*)ip_hdr + (ip_hdr->ip_hl << 2));
                srcport = ntohs(udp_hdr->source);
                dstport = ntohs(udp_hdr->dest);
            } else {
                // Other L4
                srcport = 0;
                dstport = 0;
            }
            int srcip = ntohl(ip_hdr->ip_src.s_addr);
            int dstip = ntohl(ip_hdr->ip_dst.s_addr);
            uint8_t protocol = (uint8_t)ntohs(ip_hdr->ip_p);
            memcpy(p, &srcip, sizeof(uint32_t));
            memcpy(p+sizeof(uint32_t), &dstip, sizeof(uint32_t));
            memcpy(p+sizeof(uint32_t)*2, &srcport, sizeof(uint16_t));
            memcpy(p+sizeof(uint16_t)*5, &dstport, sizeof(uint16_t));
            memcpy(p+sizeof(uint32_t)*3, &protocol, sizeof(uint8_t));
            memcpy(p+sizeof(uint8_t)*13, &iplen, sizeof(uint16_t));
            p += sizeof(uint8_t)*13+sizeof(uint16_t);
            data->cnt++;
            test += iplen;
        }
    }
    std::cout << "[Message] Read " << data->cnt << " items" << std::endl;
    std::cout << "[Message] Test = " << test << std::endl;
    pcap_close(pfile);
}

Adaptor::~Adaptor() {
    free(data->databuffer);
    free(data);
}

int Adaptor::GetNext(tuple_t* t) {
    if (data->cur > data->cnt) {
        return -1;
    }
    t->key.src_ip = *((uint32_t*)data->ptr);
    t->key.dst_ip = *((uint32_t*)(data->ptr+4));
    t->key.src_port = *((uint16_t*)(data->ptr+8));
    t->key.dst_port = *((uint16_t*)(data->ptr+10));
    t->key.protocol = *((uint16_t*)(data->ptr+12));
    t->size = *((uint16_t*)(data->ptr+13));
    data->cur++;
    data->ptr += 15;
    return 1;
}

void Adaptor::Reset() {
    data->cur = 0;
    data->ptr = data->databuffer;
}

uint64_t Adaptor::GetDataSize() {
    return data->cnt;
}
