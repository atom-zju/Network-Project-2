#include "porttable.h"
#include <netinet/in.h>
#include <string.h>

//borrowed from event.cc
//const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};


PortTable::PortTable()
{

}

/*@
  @ check and remove outdated entries
  @*/
void PortTable::set_num_ports(unsigned short ports)
{
    num_ports=ports;
    port_table=(PortEntry*)malloc(sizeof(PortEntry)*num_ports);
    for(int i=0;i<num_ports;i++){
        //at first, all the port is invalid
        port_table[i].time_stamp=-1;
    }
}

/*@
  @ check and remove outdated entries
  @*/
void PortTable::set_router_id(unsigned short router_id)
{
    id=router_id;
}

PortTable::~PortTable()
{
    free(port_table);
}

/*@
  @ check and remove outdated entries, return whether changed and the changed list
  @*/
bool PortTable::check(queue<unsigned short> &change_list)
{
    bool changed=false;
    for(int i=0;i<num_ports;i++){
        if(port_table[i].time_stamp<0)
            continue;
        if(port_table[i].time_stamp>MAX_PORT_TIMESTAMP){
            change_list.push(port_table[i].routerID);
            port_table[i].time_stamp=-1;
            changed=true;
        }
    }
    return changed;
}

/*@
  @ increase time stamp of all entries by 1
  @*/
void PortTable::inc_tstamp()
{
    for(int i=0;i<num_ports;i++){
        if(port_table[i].time_stamp<0)
            continue;
        port_table[i].time_stamp+=1;
    }
}

/*@
  @ refresh the port timestamp to 0, used when DV/LS/DATA received
  @*/
void PortTable::refresh_tstamp(unsigned short port)
{
    if(port>num_ports-1){
        std::cout<<"Err: port # exceeds in refresh_tstamp()"<<std::endl;
        return;
    }
    if(port_table[port].time_stamp<0){
        //if entry is invalid, it can only be refreshed by ping or pong message
        return;
    }
    else{
        //if entry is valid, reset the timer
        port_table[port].time_stamp=0;
    }
}

/*@
  @ given port, try to retrieve ID, return whether succeed
  @*/
bool PortTable::port2ID(unsigned short port, unsigned short& ID)
{
    if(port>num_ports-1){
        std::cout<<"Err: port # exceeds in port2ID()"<<std::endl;
        return false;
    }
    if(port_table[port].time_stamp<0){
        //if entry is invalid, return conversion fail
        return false;
    }
    else{
        //if entry is valid, reset the timer
        ID=port_table[port].routerID;
        return true;
    }
}

/*@
  @ given ID, try to retrieve port #, return whether succeeded
  @*/
bool PortTable::ID2port(unsigned short ID, unsigned short& port)
{
    for(int i=0;i<num_ports;i++){
        if(port_table[i].time_stamp<0)
            continue;
        if(port_table[i].routerID==ID){
            port=port_table[i].portNo;
            return true;
        }
    }
    return false;
}

/*@
  @ analyze ping message,return the modified packet
  @*/
void* PortTable::analysis_ping(unsigned short port, void *packet, unsigned short size)
{
    *((unsigned char*)packet)=(char)2;      //const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};
    unsigned short dest=(unsigned short) ntohs(*((unsigned short*)packet+2));
    *((unsigned short *)packet+3)=(unsigned short) htons((unsigned short)dest);
    *((unsigned short *)packet+2)=(unsigned short) htons((unsigned short)id);
    //reset the time_stamp if the corresponding entry is valid
    refresh_tstamp(port);
    return packet;
}

/*@
  @ analyze pong message,return whether table has changed
  @*/
bool PortTable::analysis_pong(unsigned short port, void *packet, unsigned int global_time,unsigned short& fromID,unsigned int& dly)
{
    unsigned short t;
    t = *((unsigned char *)packet);
    if(t!=2){
    //if(strcmp(sPacketType[t],"PONG")){
        //const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};
        // packet is not "pong" type
        std::cout<<"Err: received packet is not 'pong' type."<<std::endl;
        //free(packet);
        return false;
    }
    if(port>num_ports-1){
        std::cout<<"Err: port # exceeds in analysis_pong()"<<std::endl;
        //free(packet);
        return false;
    }
    fromID=(unsigned short) ntohs(*((unsigned short*)packet+2));
    dly=(unsigned int) ntohl(*((unsigned int*)packet+2));
    dly=global_time-dly;
    if(port_table[port].time_stamp<0||port_table[port].delay!=dly||port_table[port].routerID!=fromID){
        port_table[port].portNo=port;
        port_table[port].routerID=fromID;
        port_table[port].delay=dly;
        port_table[port].time_stamp=0;
        //free(packet);
        return true;
    }
    //else if port_table[port].time_stamp==0 && port_table[port].delay==dly, routerID==fromID, refresh time_stamp
    port_table[port].time_stamp=0;
    //free(packet);
    return false;
}

/*@
  @ make ping message,return the packet
  @*/
void* PortTable::make_pkt_ping(unsigned int global_time,unsigned short& pktsize)
{
    pktsize=sizeof(unsigned short)*4+sizeof(unsigned int);
    unsigned short* packet=(unsigned short*)malloc(pktsize);
    *(unsigned char *)packet=1; // 1 is the index of "PING" in sPacketType[] = {"DATA","PING","PONG","DV","LS"};
    //write size
    *((unsigned short *)packet+1)=(unsigned short) htons((unsigned short)pktsize);
    *((unsigned short *)packet+2)=(unsigned short) htons((unsigned short)id);
    *((unsigned int *)packet+2)=(unsigned int) htonl((unsigned int)global_time);
    return packet;
}

/*@
  @ return size of table
  @*/
unsigned short PortTable::size()
{
    return num_ports;
}

/*@
  @ get delay by port #, return whether succeeded
  @*/
bool PortTable::get_delay(unsigned short port, unsigned int &dly)
{
    if(port_table[port].time_stamp<0)
        return false;
    dly=port_table[port].delay;
    return true;
}



