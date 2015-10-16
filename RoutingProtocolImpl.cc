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
    fwdtable.set_series_num_zero();

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
    if(protocol_type==P_DV)
        *d=periodic_DV;
    else{
        *d=periodic_LS;
    }
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
    case periodic_LS:
    {
        sys->set_alarm(this,30*1000,data);
        if(ptcl==P_DV){
            unsigned short pktsize,ID;
            char* pkt;
            for(int i=0;i<porttable.size();i++){
                if(!porttable.port2ID(i,ID))
                    continue;
                pkt=(char*)fwdtable.make_pkt_DV(ID,pktsize);
                sys->send(i,pkt,pktsize);
            }
        }
        else{
            //else case, ptcl==P_LS, make LS packet and send to all ports=================================================================================================
            unsigned short pktsize;
            char* pkt;
            for(int i=0;i<porttable.size();i++){
                pkt=(char*)fwdtable.make_pkt_LS(pktsize);
                sys->send(i,pkt,pktsize);
                fwdtable.inc_series_num();
            }
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
            fwdtable.inc_tstamp();
            if(fwdtable.check()){
                //if fwdtable changed
                changed=true;
            }
        }
        else{
            //check P_LS
            fwdtable.inc_tstamp();
            if(fwdtable.check()){
                //if fwdtable changed, regenerate shortest path
                fwdtable.SP_regenerate();
            }
        }
        //check port table
        porttable.inc_tstamp();
        if(porttable.check(change_list)){
            //if there is something changed in the porttable
            while(!change_list.empty()){
                unsigned short outdatedID=change_list.front();
                if(ptcl==P_DV){
                    //if ptcl is DV
                    if(fwdtable.try_update(outdatedID,USHRT_MAX,0,outdatedID)){
                        changed=true;
                    }
                }
                else{
                    //ptcl is LS
                    //try_update_LS in fwdtable, remove all invalid neighbors=======================================================================================
                    fwdtable.try_update_LS(outdatedID,UINT_MAX);
                    changed=true;
                }
                change_list.pop();
            }
        }
        if(changed){
            //if fwdtable changed, send newest table
            if(ptcl==P_DV){
                unsigned short pktsize,ID;
                char* pkt;
                for(int i=0;i<porttable.size();i++){
                    if(!porttable.port2ID(i,ID))
                        continue;
                    pkt=(char*)fwdtable.make_pkt_DV(ID,pktsize);
                    sys->send(i,pkt,pktsize);
                }
            }
            else{
                //ptcl == LS
                //increase series number,regenerate shortest path, make LS packet, send to all ports==================================================
                fwdtable.SP_regenerate();
                unsigned short pktsize;
                char* pkt;
                for(int i=0;i<porttable.size();i++){
                    pkt=(char*)fwdtable.make_pkt_LS(pktsize);
                    sys->send(i,pkt,pktsize);
                    fwdtable.inc_series_num();
                }
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
        bool port_changed=false;
        if(porttable.get_delay(port,useddly)){
            //there used to be a record, store useddly
            porttable.analysis_pong(port,packet,sys->time(),fromID,dly);
            if(dly!=useddly)
                port_changed=true;
        }
        else{
            //there is no useddly
            porttable.analysis_pong(port,packet,sys->time(),fromID,dly);
            useddly=dly;
            port_changed=true;
        }
        if(ptcl==P_DV){
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
        }
        else{
            //ptcl == P_LS
            if(port_changed){
                //if port changed, try_update_LS, regenerate shortest path, make LS packet and send to all port==================================
                if(fwdtable.try_update_LS(fromID,dly)){
                    //if anything changed in LS table
                    fwdtable.SP_regenerate();
                    unsigned short pktsize;
                    char* pkt;
                    for(int i=0;i<porttable.size();i++){
                        pkt=(char*)fwdtable.make_pkt_LS(pktsize);
                        sys->send(i,pkt,pktsize);
                        fwdtable.inc_series_num();
                    }
                }
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
    else if(t==4){
        //else if(!strcmp(sPacketType[t],"LS")){
        //if received LS message
        //fwdtable.analysis_LS() return whether need to flood this LS message, if so, flood LS message=======================================================================
        if(fwdtable.analysis_LS(packet,size))
        {
            //if it's a new packet, update LS, flood the message
            fwdtable.SP_regenerate();
            char* pkt;
            for(int i=0;i<porttable.size();i++){
                //make a copy of the original message and send
                pkt=(char*)malloc(size);
                memcpy(pkt,packet,size);
                sys->send(i,pkt,size);
            }
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
