#include "inputadaptor.hpp"
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <IPv4Layer.h>
#include <TcpLayer.h>
#include <UdpLayer.h>
#include <Packet.h>
#include <PcapFileDevice.h>
#include <arpa/inet.h>

InputAdaptor::InputAdaptor(std::string filename, uint64_t buffersize) {
    data = (adaptor_t*)calloc(1, sizeof(adaptor_t));
    data->databuffer = (unsigned char*)calloc(buffersize, sizeof(unsigned char));
    data->ptr = data->databuffer;
    data->cnt = 0;
    data->cur = 0;
    //Read pcap file
    std::string path = filename;
    pcpp::PcapFileReaderDevice reader(path.c_str());
    if (!reader.open()) {
        std::cout << "[Error] Fail to open pcap file" << std::endl;
        exit(-1);
    }
    unsigned char* p = data->databuffer;
    pcpp::RawPacket rawPacket;
    while (reader.getNextPacket(rawPacket)) {
        pcpp::Packet parsedPacket(&rawPacket);
        pcpp::IPv4Layer* ipLayer = parsedPacket.getLayerOfType<pcpp::IPv4Layer>();
        if (p+sizeof(tuple_t) < data->databuffer + buffersize) {
            if (!parsedPacket.isPacketOfType(pcpp::IPv4)) continue;
            pcpp::Layer* nextLayer = ipLayer->getNextLayer();
            int srcport = 0, dstport = 0;
            if (nextLayer == NULL);
            else if (nextLayer->getProtocol() == pcpp::TCP) {
                srcport = (int)ntohs(((pcpp::TcpLayer* )nextLayer)->getTcpHeader()->portSrc);
                dstport = (int)ntohs(((pcpp::TcpLayer* )nextLayer)->getTcpHeader()->portDst);
            } else if (nextLayer->getProtocol() == pcpp::UDP) {
                srcport = (int)ntohs(((pcpp::UdpLayer* )nextLayer)->getUdpHeader()->portSrc);
                dstport = (int)ntohs(((pcpp::UdpLayer* )nextLayer)->getUdpHeader()->portDst);
            } else ;
            memcpy(p, &(ipLayer->getIPv4Header()->ipSrc), sizeof(uint32_t));
            memcpy(p+sizeof(uint32_t), &(ipLayer->getIPv4Header()->ipDst), sizeof(uint32_t));
            memcpy(p+sizeof(uint32_t)*2, &srcport, sizeof(uint16_t));
            memcpy(p+sizeof(uint16_t)*5, &dstport, sizeof(uint16_t));
            memcpy(p+sizeof(uint32_t)*3, &(ipLayer->getIPv4Header()->protocol), sizeof(uint8_t));
            memcpy(p+sizeof(uint8_t)*13, &(ipLayer->getIPv4Header()->totalLength), sizeof(uint16_t));
            p += sizeof(uint8_t)*13+sizeof(uint16_t);
            data->cnt++;
        }  else break;
    }
    std::cout << "[Message] Read " << data->cnt << " items" << std::endl;
    reader.close();
}

InputAdaptor::~InputAdaptor() {
    free(data->databuffer);
    free(data);
}

int InputAdaptor::GetNext(tuple_t* t) {
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

void InputAdaptor::Reset() {
    data->cur = 0;
    data->ptr = data->databuffer;
}

uint64_t InputAdaptor::GetDataSize() {
    return data->cnt;
}
