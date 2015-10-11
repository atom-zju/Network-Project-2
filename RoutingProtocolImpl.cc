#include "RoutingProtocolImpl.h"

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type)
    :fwdtable(router_id,protocol_type),porttable(num_ports,router_id),ptcl(protocol_type),id(router_id) {
  // add your own code

    //set up ping alarm
    AlarmType *d=malloc(sizeof(AlarmType));
    *d=periodic_PING;
    sys->set_alarm(this,0,d);

    //set up 1 sec check alarm
    d=malloc(sizeof(AlarmType));
    *d=one_sec_check;
    sys->set_alarm(this,1*1000,d);

    //set up DV alarm
    d=malloc(sizeof(AlarmType));
    *d=periodic_DV;
    sys->set_alarm(this,30*1000,d);
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code
    switch((AlarmType)(*data)){

    case periodic_PING:
    {
        sys->set_alarm(this,10*1000,data);
        //send ping message to all the ports
        unsigned short pktsize;
        char* pkt;
        for(int i=0;i<porttable.size();i++){
            pkt=porttable.make_pkt_ping(sys->time(),pktsize);
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
            pkt=fwdtable.make_pkt_DV(ID,pktsize);
            sys->send(i,pkt,pktsize);
        }
        break;
    }
    case one_sec_check:
    {
        sys->set_alarm(this,1*1000,data);
        //check port table
        porttable.inc_tstamp();
        porttable.check();
        //check forward table
        if(ptcl==DV){
            fwdtable.inc_tstamp_DV();
            fwdtable.check_DV();
        }
        else{
            //check LS
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
    if(!strcmp(sPacketType[t],"PING")){
        //if you receive 'ping message'
        char* pkt=porttable.analysis_ping(port,packet,size);
        sys->send(port,pkt,size);
    }
    else if(!strcmp(sPacketType[t],"PONG")){
        //if received pong message
        unsigned short fromID;
        unsigned int dly;
        porttable.analysis_pong(port,packet,sys->time(),fromID,dly);
        /****************************************/
        if(fwdtable.try_update(fromID)){
            //if fwdtable has changed, send DV to all ports
            unsigned short pktsize,ID;
            char* pkt;
            for(int i=0;i<porttable.size();i++){
                if(!porttable.port2ID(i,ID))
                    continue;
                pkt=fwdtable.make_pkt_DV(ID,pktsize);
                sys->send(i,pkt,pktsize);
            }
        }
    }
    else if(!strcmp(sPacketType[t],"DV")){
        //if received DV message
        /************************************************/
        if(fwdtable.analysis_DV(packet,size)){
            //if fwdtable has changed, send DV to all ports
            unsigned short pktsize,ID;
            char* pkt;
            for(int i=0;i<porttable.size();i++){
                if(!porttable.port2ID(i,ID))
                    continue;
                pkt=fwdtable.make_pkt_DV(ID,pktsize);
                sys->send(i,pkt,pktsize);
            }
        }
    }
    else if(!strcmp(sPacketType[t],"DATA")){
        //if received DATA message
        unsigned short nextID,nextPort;
        if(fwdtable.analysis_data(packet,size,nextID)){
            //find next router id
            if(nextID==id){
                //reached destination
                std::cout<<"received DATA"<<std::endl;
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
