#ifndef FWDTABLE_H
#define FWDTABLE_H

#include "global.h"

#define MAX_DV_TIMESTAMP 45


//this is borrowed from event.cc
//const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};

// forward table entry structure
struct FwdEntry{
    FwdEntry(){};
    FwdEntry(unsigned short destID, unsigned int cost, unsigned short next_hop) : destID(destID), cost(cost), via_hop(next_hop), time_stamp(0) {};
    unsigned short destID;
    unsigned int cost;
    unsigned short via_hop;    //in LS protocol, via_hop is the source router
    short int time_stamp;       //time stamp can also function as a seriel number in LS protocol
};

class FwdTable
{
public:
    FwdTable();
    void set_router_id(unsigned short router_id);
    void set_protocol(eProtocolType protocol);
    bool check_DV();       //check on 1 sec period, remove the outdated entry (timestamp larger than 15), return whether changed
    void inc_tstamp_DV();  //increase timestamps of all valid entries by 1
    bool analysis_DV(void *packet, unsigned short size, unsigned int delay);  //analyse DV packet if do any update, return true, else return false
    bool analysis_LS(unsigned short fromID, void *packet, unsigned short size);  //analyse LS packet
    void* make_pkt_DV(unsigned short toID, unsigned short& pktsize);        //make the DV packet
    void* make_pkt_LS();        //make the LS packet
    //FwdEntry retrieve(unsigned short destID);       //retrieve the entry corresponding to certain dest router
    bool try_update(unsigned short desID, unsigned int cst, unsigned int usedcst, unsigned short nextHop);     //try to update one entry, used when pong received
    bool analysis_data(void *packet, unsigned short size, unsigned short &nextID);
    int size(){return fwd_table.size();}
private:
    eProtocolType ptcl;
    hash_map<int, vector<FwdEntry> > fwd_table;
    unsigned short id;
};

#endif // FWDTABLE_H
