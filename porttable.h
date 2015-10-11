#ifndef PORTTABLE_H
#define PORTTABLE_H

#include "global.h"

#define MAX_PORT_TIMESTAMP 15

//borrowed from event.cc
const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};


struct PortEntry{
    PortEntry(unsigned short portNo,unsigned short routerID,unsigned int delay):
        portNo(portNo), routerID(routerID),delay(delay), time_stamp(0){};
    unsigned short portNo;
    unsigned short routerID;
    unsigned int delay;
    short int time_stamp;
};

class PortTable
{
public:
    PortTable(unsigned short num_ports, unsigned short router_id);
    ~PortTable();   //free the port_table pointer
    void check();   //check and remove all the outdated entries
    void inc_tstamp();  //increase the timestamp by 1
    void* analysis_ping(unsigned short port, void *packet, unsigned short size);
    bool analysis_pong(unsigned short port, void *packet, unsigned int global_time,unsigned short& fromID, unsigned int &dly);
    void refresh_tstamp(unsigned short port);       //refresh the port timestamp to 0, used when DV/LS/DATA received
    bool port2ID(unsigned short port, unsigned short &ID);  //given port, try to retrieve ID, return whether succeed
    bool ID2port(unsigned short ID, unsigned short &port);  //given ID, try to retrieve port, return whether succeed
    void* make_pkt_ping(unsigned int global_time,unsigned short& pktsize);      //make ping pkt
    unsigned short size();      //return the size of table

private:
    unsigned short num_ports;
    unsigned short id;
    PortEntry* port_table;
};

#endif // PORTTABLE_H
