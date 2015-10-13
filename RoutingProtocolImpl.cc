#include "RoutingProtocolImpl.h"
#include "Node.h"
#include <string.h>
#include <limits.h>


//borrowed from event.cc
//const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type)
{
  // add your own code
    ptcl=protocol_type;
    id=router_id;

    porttable.set_num_ports(num_ports);
    porttable.set_router_id(router_id);

    fwdtable.set_protocol(protocol_type);
    fwdtable.set_router_id(router_id);

    //set up ping alarm
    AlarmType *d=(AlarmType *)malloc(sizeof(AlarmType));
    *d=periodic_PING;
    sys->set_alarm(this,0,d);

    //set up 1 sec check alarm
    d=(AlarmType *)malloc(sizeof(AlarmType));
    *d=one_sec_check;
    sys->set_alarm(this,1*1000,d);

    //set up DV alarm
    d=(AlarmType *)malloc(sizeof(AlarmType));
    *d=periodic_DV;
    sys->set_alarm(this,30*1000,d);
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code
    switch((AlarmType)(*(AlarmType*)data)){

    case periodic_PING:
    {
        sys->set_alarm(this,10*1000,data);
        //send ping message to all the ports
        unsigned short pktsize;
        char* pkt;
        for(int i=0;i<porttable.size();i++){
            pkt=(char*)porttable.make_pkt_ping(sys->time(),pktsize);
            sys->send(i,pkt,pktsize);
        }
        break;
    }
    case periodic_DV:
    {
        sys->set_alarm(this,30*1000,data);
        unsigned short pktsize,ID;
        char* pkt;
        for(int i=0;i<porttable.size();i++){
            if(!porttable.port2ID(i,ID))
                continue;
            pkt=(char*)fwdtable.make_pkt_DV(ID,pktsize);
            sys->send(i,pkt,pktsize);
        }
        break;
    }
    case one_sec_check:
    {
        sys->set_alarm(this,1*1000,data);
        queue<unsigned short> change_list;
        bool changed=false;     //indicate whether fwdtable has changed
        //check forward table
        if(ptcl==P_DV){
            fwdtable.inc_tstamp_DV();
            if(fwdtable.check_DV()){
                //if fwdtable changed
                changed=true;
            }
        }
        else{
            //check P_LS
        }
        //check port table
        porttable.inc_tstamp();
        if(porttable.check(change_list)){
            //if there is something changed in the porttable
            while(!change_list.empty()){
                unsigned short outdatedID=change_list.front();
                if(fwdtable.try_update(outdatedID,USHRT_MAX,0,outdatedID)){
                    changed=true;
                }
                change_list.pop();
            }
        }
        if(changed){
            //if fwdtable changed, send newest table
            unsigned short pktsize,ID;
            char* pkt;
            for(int i=0;i<porttable.size();i++){
                if(!porttable.port2ID(i,ID))
                    continue;
                pkt=(char*)fwdtable.make_pkt_DV(ID,pktsize);
                sys->send(i,pkt,pktsize);
            }
        }
        break;
    }
    }
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  // add your own code
    //const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};
    unsigned short t;
    t = *((unsigned char *)packet);
    if(t==1){
    //if(!strcmp(sPacketType[t],"PING")){
        //const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};
        //if you receive 'ping message'
        char* pkt=(char*)porttable.analysis_ping(port,packet,size);
        sys->send(port,pkt,size);
    }
    else if(t==2){
    //else if(!strcmp(sPacketType[t],"PONG")){
        //if received pong message
        unsigned short fromID;
        unsigned int dly,useddly;
        if(porttable.get_delay(port,useddly)){
            //there used to be a record, store useddly
            porttable.analysis_pong(port,packet,sys->time(),fromID,dly);
        }
        else{
            //there is no useddly
            porttable.analysis_pong(port,packet,sys->time(),fromID,dly);
            useddly=dly;
        }
        if(fwdtable.try_update(fromID,dly,useddly,fromID)){
            //if fwdtable has changed, send DV to all ports
            unsigned short pktsize,ID;
            char* pkt;
            for(int i=0;i<porttable.size();i++){
                if(!porttable.port2ID(i,ID))
                    continue;
                pkt=(char*)fwdtable.make_pkt_DV(ID,pktsize);
                sys->send(i,pkt,pktsize);
            }
        }
        free(packet);
    }
    else if(t==3){
    //else if(!strcmp(sPacketType[t],"DV")){
        //if received DV message
        unsigned int dly;
        if(porttable.get_delay(port,dly)){
            //get delay succeeded
            if(fwdtable.analysis_DV(packet,size,dly)){
                //if fwdtable has changed, send DV to all ports
                unsigned short pktsize,ID;
                char* pkt;
                for(int i=0;i<porttable.size();i++){
                    if(!porttable.port2ID(i,ID))
                        continue;
                    pkt=(char*)fwdtable.make_pkt_DV(ID,pktsize);
                    sys->send(i,pkt,pktsize);
                }
            }
        }
        else{
            //get delay failed, do nothing
        }
    free(packet);
    }
    else if(t==0){
    //else if(!strcmp(sPacketType[t],"DATA")){
        //if received DATA message
        unsigned short nextID,nextPort;
        if(fwdtable.analysis_data(packet,size,nextID)){
            //find next router id
            if(nextID==id){
                //reached destination
                //std::cout<<"received DATA"<<std::endl;
                free(packet);
            }
            else if(porttable.ID2port(nextID,nextPort)){
                //if find next port
                sys->send(nextPort,packet,size);
            }
            else{
                std::cout<<"Err: cannot find next port for DATA"<<std::endl;
                free(packet);
            }
        }
        else{
            //next router id not found
            std::cout<<"Err: cannot find next router for DATA, lost packet"<<std::endl;
            free(packet);
        }
    }
}

// add more of your own code
