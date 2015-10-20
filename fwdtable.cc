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
bool FwdTable::check()
{
    bool changed=false;
    queue<int> clear_vec;
    for(hash_map<int, vector<FwdEntry> >::iterator it=fwd_table.begin(); it!=fwd_table.end(); it++){
        if(!(*it).second.empty()){
            if((*it).second.at(0).time_stamp>MAX_DV_LS_TIMESTAMP){
                //fwd_table.erase((*it).first);
                clear_vec.push((*it).first);
            }
        }
    }
    while(!clear_vec.empty()){
        int entry_num=clear_vec.front();
        if(entry_num!=id){
            //do not clear self entry
            fwd_table[entry_num].clear();
            fwd_table.erase(entry_num);
        }
        clear_vec.pop();
        changed=true;
    }
    return changed;
}

/*@
  @ increase the time stamp of each entry by 1
  @*/
void FwdTable::inc_tstamp()
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
    
    //contains_vec  is used to contain all the entries in DV packet
    std::vector<unsigned short> contains_vec;

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

	//put desID into the contains_vec
	contains_vec.push_back(desID);

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
	      if (fwd_table[desID].at(0).via_hop==nextHop) {
		//although new path is no shorter than previous one, if it is the same path, update delay and time_stamp
		fwd_table[desID].at(0).time_stamp=0;
		if(delay+cst!=fwd_table[desID].at(0).cost){
		  fwd_table[desID].at(0).cost=delay+cst;
		  changed=true;
		}
		
	      }
                //new path is longer than previous one, do nothing
            }
        }
    }
    //clear vec is used to clear entries up
    std::queue<unsigned short> clear_vec;

    for(hash_map<int, vector<FwdEntry> >::iterator it=fwd_table.begin(); it!=fwd_table.end(); it++){
        if((*it).second.empty())
          continue;
	if((*it).second.at(0).via_hop==nextHop && (*it).second.at(0).via_hop!=(*it).second.at(0).destID){
	  //if previously we use nextHop to reach destination, make sure destID is in the contains_vec
	  unsigned short j=0;
	  for(;j<contains_vec.size();j++){
	    if((*it).second.at(0).destID ==  contains_vec.at(j)){
	      //if find destID in the DV packet
	      break;
	    }
	  }
	  if(j==contains_vec.size()){
	    //if entry not found in contains_vec
	    clear_vec.push((*it).first);
	  }
	}
    }

    //clear the clear_vec
    while(!clear_vec.empty()){
      int entry_num=clear_vec.front();
      if(entry_num!=id){
	//do not clear self entry
	fwd_table[entry_num].clear();
	fwd_table.erase(entry_num);
      }
      clear_vec.pop();
      changed=true;
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
            changed=true;
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
	  //if((*it).second.at(0).destID!=nextHop)
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
        if(nextID==USHRT_MAX){
            //has the entry, but cannot reach
            return false;
        }
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

/*@
  @ increase the series num by 1
  @*/
void FwdTable::inc_series_num()
{
    seriesNum++;
}

/*@
  @ set series num to zero
  @*/
void FwdTable::set_series_num_zero()
{
    seriesNum=0;
}

/*@
  @ analyse LS packet, return whether need to flood this message
  @*/
bool FwdTable::analysis_LS(void *packet, unsigned short size)
{
    unsigned short t;
    bool changed=false;
    t = *((unsigned char *)packet);
    if(t!=4){
    //const char *sPacketType[] = {"DATA","PING","PONG","DV","LS"};
    //if(strcmp(sPacketType[t],"LS")){
        // packet is not "LS" type
        std::cout<<"Err: received packet is not 'LS' type."<<std::endl;
        //free(packet);
        return false;
    }
    if(size<12){
        // size is small than minimal size
        std::cout<<"Err: received 'LS' packet is too small."<<std::endl;
        //free(packet);
        return false;
    }

    //get router ID from the packet
    unsigned short routerID=(unsigned short) ntohs(*((unsigned short*)packet+2));
    unsigned int SN=(unsigned int) ntohl(*((unsigned int*)packet+2));

    if(fwd_table.find(routerID) != fwd_table.end()){
        //if there is an entry in the table
        if(!fwd_table[routerID].empty()){
            //if the entry is not emtpty
            if(fwd_table[routerID].at(0).s_num>=SN){
                //if series number has seen before
                return false;
            }
        }
    }

    //else case: it is the newest series num, reconstruct neighbor vector
    fwd_table[routerID].clear();
    changed=true;

    unsigned short pass=size/4-3;

    for(int i=0;i<pass;i++){

        //get neighbor router ID and cost
        unsigned short neighborID=(unsigned short) ntohs(*((unsigned short*)packet+6+2*i));
        unsigned short cst=(unsigned short) ntohs(*((unsigned short*)packet+7+2*i));
        //in LS, neighbor ID will be stored in destID and next hop will be infinity
        fwd_table[routerID].push_back(FwdEntry(neighborID,cst,USHRT_MAX,SN));
    }

    return changed;
}

/*@
  @ make LS packet
  @*/
void* FwdTable::make_pkt_LS(unsigned short& pktsize)
{
    char* packet;
    if(fwd_table.find(id)==fwd_table.end()){
        //cannot find self entry
        std::cout<<"cannot find self entry in LS table"<<std::endl;
    }
    pktsize=12+4*fwd_table[id].size();
    packet=(char*)malloc(pktsize);

    //write packet type
    *(unsigned char *)packet=4; // 3 is the index of "LS" in sPacketType[] = {"DATA","PING","PONG","DV","LS"};
    //write total size
    *((unsigned short *)packet+1)=(unsigned short) htons((unsigned short)pktsize);
    //write source id
    *((unsigned short *)packet+2)=(unsigned short) htons((unsigned short)id);
    //write series number
    *((unsigned int *)packet+2)=(unsigned int) htonl((unsigned int)seriesNum);

    //write node-cost pair
    for(unsigned int i=0;i<fwd_table[id].size();i++){
        //wirte neighbor node id
        *((unsigned short *)packet+6+2*i)=(unsigned short) htons((unsigned short)fwd_table[id].at(i).destID);
        //write cost
        *((unsigned short *)packet+7+2*i)=(unsigned short) htons((unsigned short)fwd_table[id].at(i).cost);
    }
    return (void*)packet;
}

/*@
  @ try to update LS table, used when received pong, or check() timeout
  @*/
bool FwdTable::try_update_LS(unsigned short toID,unsigned int cst)
{
    for(unsigned int i=0;i<fwd_table[id].size();i++){
        if(fwd_table[id].at(i).destID==toID){
            //find corresponding entry with toID
            if(cst==USHRT_MAX){
                //if cst == infinity, eleminate entry
                fwd_table[id].erase(fwd_table[id].begin()+i);
                return true;
            }
            if(fwd_table[id].at(i).cost!=cst){
                //if cost!=cst,table changed, return true
                fwd_table[id].at(i).cost=cst;
                return true;
            }
            else{
                //table not changed, return false
                return false;
            }
        }
    }
    //do not contains corresponding fwdentry
    if(cst!=USHRT_MAX){
        fwd_table[id].push_back(FwdEntry(toID,cst,id,0));
        return true;
    }
    else{
        //entry try to eleminate does not exist
        return false;
    }
}

/*@
  @ regenerate the shortest path algorithm using dijastra algorithm
  @*/
bool FwdTable::SP_regenerate()
{
    int max_key=0;
    bool changed=false;
    //find max router id in the hash map
    for(hash_map<int, vector<FwdEntry> >::iterator it=fwd_table.begin(); it!=fwd_table.end(); it++){
        if((*it).first>max_key){
            max_key=(*it).first;
        }
    }

    //dist is used to store the distance, path_to is used to store parant node
    unsigned int dist[max_key+1];
    unsigned short path_to[max_key+1];
    bool marked[max_key+1];

    //initilize two array
    for(int i=0; i<max_key+1;i++){
        dist[i]=UINT_MAX;
        marked[i]=false;
        path_to[i]=USHRT_MAX;
    }

    //dist to self node is 0
    dist[id]=0;
    //path_to[id]=USHRT_MAX;


    //begin construct shortest path tree
    while(true){

        unsigned int min_dis=UINT_MAX;
        unsigned short min_dis_idx;


        //find the unvisited node with the shortest dist
        for(int i=0;i<max_key+1;i++){
            if(dist[i]<min_dis && marked[i]==false){
                min_dis=dist[i];
                min_dis_idx=i;
            }
        }

        if(min_dis==UINT_MAX){
            //tranversed all the possible nodes, not found anything, terminate
            break;
        }

        marked[min_dis_idx]=true;
        if(fwd_table.find(min_dis_idx)==fwd_table.end())
            continue;
        if(fwd_table[min_dis_idx].empty()){
            continue;
        }


        //relax each adjacent node of min_dist_idx
        for(unsigned int i=0;i<fwd_table[min_dis_idx].size();i++){
            //prevent crash if destID is larger than max_key
            if(fwd_table[min_dis_idx].at(i).destID>max_key)
                continue;
            if(dist[fwd_table[min_dis_idx].at(i).destID]>dist[min_dis_idx]+fwd_table[min_dis_idx].at(i).cost){
                //if find a shorter path
                dist[fwd_table[min_dis_idx].at(i).destID]=dist[min_dis_idx]+fwd_table[min_dis_idx].at(i).cost;
                path_to[fwd_table[min_dis_idx].at(i).destID]=min_dis_idx;
            }
        }
    }
    //by now SPT has been constructed, store the direct neighbor into via_hop
    for(hash_map<int, vector<FwdEntry> >::iterator it=fwd_table.begin(); it!=fwd_table.end(); it++){
        unsigned short i=(*it).first;
        for(;path_to[i]!=id;i=path_to[i]){
            //moving in the SPT backwards to find the first direct neighbors of self
            if(path_to[i]==USHRT_MAX){
                //has the entry, but cannot reach
                i=USHRT_MAX;
                break;
            }
        }

        if((*it).second.empty()){
            continue;
        }

        if((*it).second.at(0).via_hop!=i){
            //changed the forward table
            (*it).second.at(0).via_hop=i;
            changed=true;
        }
    }
    return changed;
}
