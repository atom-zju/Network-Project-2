#include "fwdtable.h"
#include <netinet/in.h>
#include <string.h>
#include <limits.h>

//borrowed from event.cc
//const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};

FwdTable::FwdTable()
{

}

/*@
  @ set protocol
  @*/
void FwdTable::set_protocol(eProtocolType protocol)
{
    ptcl=protocol;
}

/*@
  @ set router id
  @*/
void FwdTable::set_router_id(unsigned short router_id)
{
    id=router_id;
}

/*@
  @ check and remove all the outdated entries, return whether changed
  @*/
bool FwdTable::check_DV()
{
    bool changed=false;
    queue<int> clear_vec;
    for(hash_map<int, vector<FwdEntry> >::iterator it=fwd_table.begin(); it!=fwd_table.end(); it++){
        if(!(*it).second.empty()){
            if((*it).second.at(0).time_stamp>MAX_DV_TIMESTAMP){
                //fwd_table.erase((*it).first);
                clear_vec.push((*it).first);
            }
        }
    }
    while(!clear_vec.empty()){
        int entry_num=clear_vec.front();
        fwd_table[entry_num].clear();
        fwd_table.erase(entry_num);
        clear_vec.pop();
        //changed=true;
    }
    return changed;
}

/*@
  @ increase the time stamp of each entry by 1
  @*/
void FwdTable::inc_tstamp_DV()
{
    for(hash_map<int, vector<FwdEntry> >::iterator it=fwd_table.begin(); it!=fwd_table.end(); it++){
        if(!(*it).second.empty())
            (*it).second.at(0).time_stamp++;
    }
}

/*@
  @ analyse DV packet if do any update, return true, else return false
  @*/
bool FwdTable::analysis_DV(void *packet, unsigned short size,unsigned int delay)
{
    unsigned short t;
    bool changed=false;
    t = *((unsigned char *)packet);
    if(t!=3){
    //const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};
    //if(strcmp(sPacketType[t],"DV")){
        // packet is not "DV" type
        std::cout<<"Err: received packet is not 'DV' type."<<std::endl;
        //free(packet);
        return false;
    }
    if(size<8){
        // size is small than minimal size
        std::cout<<"Err: received 'DV' packet is too small."<<std::endl;
        //free(packet);
        return false;
    }

    //next hop is the source router in the packet
    unsigned short nextHop=(unsigned short) ntohs(*((unsigned short*)packet+2));

    unsigned short pass=size/4-2;

    //start parse packet content
    for(int i=0;i<pass;i++){

        //get dest router ID and cost
        unsigned short desID=(unsigned short) ntohs(*((unsigned short*)packet+4+2*i));
        if(desID==id){
            //if the entry is about how to reach the node itself, ignore
            continue;
        }
        unsigned short cst=(unsigned short) ntohs(*((unsigned short*)packet+5+2*i));
        if(cst==USHRT_MAX){
            //poison reverse info, ignore
            continue;
        }

        if(fwd_table.find(desID) == fwd_table.end()){
        //do not have path to desID previously, insert a new entry
            if(fwd_table[desID].empty())
                fwd_table[desID].push_back(FwdEntry());
            fwd_table[desID].at(0).cost=delay+cst;
            fwd_table[desID].at(0).destID=desID;
            fwd_table[desID].at(0).time_stamp=0;
            fwd_table[desID].at(0).via_hop=nextHop;
            changed=true;
        }
        //else case, there is an entry to desID
        else{
            if(fwd_table[desID].empty()){
                fwd_table[desID].push_back(FwdEntry());
                fwd_table[desID].at(0).cost=delay+cst;
                fwd_table[desID].at(0).destID=desID;
                fwd_table[desID].at(0).time_stamp=0;
                fwd_table[desID].at(0).via_hop=nextHop;
            }
            else if(delay+cst<fwd_table[desID].at(0).cost){
                //new path is shorter than the previous one, change entry
                fwd_table[desID].at(0).cost=delay+cst;
                fwd_table[desID].at(0).destID=desID;
                fwd_table[desID].at(0).time_stamp=0;
                fwd_table[desID].at(0).via_hop=nextHop;
                changed=true;
            }
            else{
                //new path is longer than previous one, do nothing
            }
        }
    }
    //free(packet);
    return changed;
}


/*@
  @ make the DV packet return the packet and size
  @*/
void* FwdTable::make_pkt_DV(unsigned short toID, unsigned short& pktsize)
{
    pktsize=8+4*fwd_table.size();
    short* packet=(short*)malloc(pktsize);


    //write packet type
    *(unsigned char *)packet=3; // 3 is the index of "DV" in sPacketType[] = {"DATA","PING","PONG","DV","LS"};
    //write total size
    *((unsigned short *)packet+1)=(unsigned short) htons((unsigned short)pktsize);
    //write source id
    *((unsigned short *)packet+2)=(unsigned short) htons((unsigned short)id);
    //write dest id
    *((unsigned short *)packet+3)=(unsigned short) htons((unsigned short)toID);
    //write node - cost pair
    int i=0;
    for(hash_map<int, vector<FwdEntry> >::iterator it=fwd_table.begin(); it!=fwd_table.end(); it++,i+=2){
        if((*it).second.empty())
            continue;
        //wirte node id
        *((unsigned short *)packet+4+i)=(unsigned short) htons((unsigned short)(*it).second.at(0).destID);
        //write cost
        if((*it).second.at(0).via_hop!=toID){
            //if via hop is not dest router, write cost
            *((unsigned short *)packet+5+i)=(unsigned short) htons((unsigned short)(*it).second.at(0).cost);
        }
        else{
            //if via hop is dest router, poison reverse
            *((unsigned short *)packet+5+i)=(unsigned short) htons((unsigned short)USHRT_MAX);
        }
    }
    return (void*)packet;
}

/*@
  @ retrieve the entry corresponding to certain dest router
  @*/
//FwdEntry FwdTable::retrieve(unsigned short destID)
//{
//    if(fwd_table.find(destID) == fwd_table.end()){
//        //entry not found
//        return NULL;
//    }
//    return fwd_table[destID];
//}


/*@
  @ retrieve the entry corresponding to certain dest router, returned bool variable indicate whether table has changed
  @*/
bool FwdTable::try_update(unsigned short desID, unsigned int cst,unsigned int usedcst, unsigned short nextHop)
{
    bool changed=false;
    if(cst==USHRT_MAX){
        //if cst == USHRT_MAX, remove all the entry related with destID
        queue<int> clear_vec;
        for(hash_map<int, vector<FwdEntry> >::iterator it=fwd_table.begin(); it!=fwd_table.end(); it++){
            if((*it).second.empty())
                continue;
            if((*it).second.at(0).via_hop==desID){
                    clear_vec.push((*it).first);
            }
        }
        while(!clear_vec.empty()){
            unsigned short rmID=clear_vec.front();
            fwd_table[rmID].clear();
            fwd_table.erase(rmID);
            clear_vec.pop();
            //changed=true;
        }
        return changed;
    }
    //if it is just a normal update
    if(fwd_table.find(desID) == fwd_table.end()){
        //entry not found, insert entry
        if(fwd_table[desID].empty())
            fwd_table[desID].push_back(FwdEntry());
        fwd_table[desID].at(0).cost=cst;
        fwd_table[desID].at(0).destID=desID;
        fwd_table[desID].at(0).time_stamp=0;
        fwd_table[desID].at(0).via_hop=nextHop;
        changed=true;
    }
    else if(fwd_table[desID].empty()){
        fwd_table[desID].push_back(FwdEntry());
        fwd_table[desID].at(0).cost=cst;
        fwd_table[desID].at(0).destID=desID;
        fwd_table[desID].at(0).time_stamp=0;
        fwd_table[desID].at(0).via_hop=nextHop;
        changed=true;
    }
    else if(fwd_table[desID].at(0).via_hop!=nextHop){
        if(cst<fwd_table[desID].at(0).cost){
            //find a shorter path, change path
            fwd_table[desID].at(0).cost=cst;
            fwd_table[desID].at(0).destID=desID;
            fwd_table[desID].at(0).time_stamp=0;
            fwd_table[desID].at(0).via_hop=nextHop;
            changed=true;
        }
        else{
            //current path is shorter, do nothing
            //return false;
        }
    }
        //refresh all entries whose via_hop == nextHop
    for(hash_map<int, vector<FwdEntry> >::iterator it=fwd_table.begin(); it!=fwd_table.end(); it++){
        if((*it).second.empty())
            continue;
        if((*it).second.at(0).via_hop==nextHop){ //&& {
		if((*it).second.at(0).destID!=nextHop)
            		(*it).second.at(0).cost=(*it).second.at(0).cost+cst-usedcst;
            (*it).second.at(0).time_stamp=0;
            if(cst!=usedcst)
                changed=true;
        }
    }

        return changed;
}

/*@
  @ analysis data message return next router id
  @*/
bool FwdTable::analysis_data(void *packet, unsigned short size, unsigned short& nextID)
{
    unsigned short dest;
    dest=(unsigned short) ntohs(*((unsigned short*)packet+3));
    if(dest==id){
        //reached destination
        nextID=dest;
        return true;
    }
    if(fwd_table.find(dest) == fwd_table.end()){
        //can not reach next ID
        return false;
    }
    else{
        if(fwd_table[dest].empty())
            return false;
        nextID=fwd_table[dest].at(0).via_hop;
        return true;
    }
}

/*@
  @ return the size of the hash table
  @*/
int FwdTable::size()
{
    return fwd_table.size();
}
