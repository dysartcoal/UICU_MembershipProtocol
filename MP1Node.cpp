/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
    memset(&this->ping.addr, 0, sizeof(char[6]));
    memset(&this->indping.addr, 0, sizeof(char[6]));
    initFailedList();
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
    char *ptr;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        /*
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));
         */
        
        createMessageHdr(msg, JOINREQ, &memberNode->addr, &memberNode->heartbeat, &ptr);
        cout<<"Sending JOINREQ: "<< memberNode->addr.addr <<" heartbeat: " << memberNode->heartbeat << endl;

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    memberNode->heartbeat++;
    
    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
    MessageHdr msgHdr;
    Address peeraddr;
    long heartbeat;
    MemberListEntry *mle;
    Address addr;
    int membercnt = 0;
    char *ptr;
    
    getSenderInfo(data, &msgHdr, &peeraddr, &heartbeat, &ptr);
    cout << "memberNode address: " << memberNode->addr.getAddress() << " Curr Time: " << this->par->getcurrtime() << endl;
    cout << "memberNode pingcounter: " << memberNode->pingCounter << " Timeoutcounter: " << memberNode->timeOutCounter << endl;
    
    if (msgHdr.msgType == JOINREQ) {
        cout<<"JOINREQ: "<<peeraddr.getAddress() <<" heartbeat: " << heartbeat << endl;
        mle = (MemberListEntry *) malloc(sizeof(MemberListEntry));
        updateMLEFromValues(mle, &peeraddr, &heartbeat, &memberNode->heartbeat);
        addMember(mle);
        sendJOINREP(&peeraddr, memberNode->memberList);
        //sendJOINREP(&peeraddr);
        
    } else if (msgHdr.msgType == JOINREP) {
        cout<<"JOINREP: "<<peeraddr.getAddress()  <<" heartbeat: " << heartbeat << endl;
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Joining a group...");
#endif
        memberNode->inGroup = true;
        
        // The MemberListEntry items are appended to the end of the message.
        // Work out how many there are.
        membercnt = (size - sizeof(int) - sizeof(peeraddr.addr) - sizeof(long))/sizeof(MemberListEntry);
        
        for (int i = 0; i < membercnt; i++) {
            mle = (MemberListEntry *) malloc(sizeof(MemberListEntry));
            mle->setid((int) *ptr);
            ptr += sizeof(int);
            mle->setport((short) *ptr);
            ptr += sizeof(short);
            mle->setheartbeat((long) *ptr);
            ptr += sizeof(long);
            mle->settimestamp(memberNode->heartbeat);
            addMember(mle);
        }
    } else if (msgHdr.msgType == PING) {
        cout<<"PING: "<<peeraddr.getAddress()  <<" heartbeat: " << heartbeat << endl;
    #ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Received a ping...");
    #endif
        
        // The MemberListEntry items are appended to the end of the message.
        // Work out how many there are.
        membercnt = (size - sizeof(int) - sizeof(peeraddr.addr) - sizeof(long))/sizeof(MemberListEntry);
        
        for (int i = 0; i < membercnt; i++) {
            mle = (MemberListEntry *) malloc(sizeof(MemberListEntry));
            mle->setid((int) *ptr);
            ptr += sizeof(int);
            mle->setport((short) *ptr);
            ptr += sizeof(short);
            mle->setheartbeat((long) *ptr);
            ptr += sizeof(long);
            mle->settimestamp(memberNode->heartbeat);
            addMember(mle);
        }
    }
    
    return true;
    
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
    int toind;
    Address toaddr;

    if (memberNode->timeOutCounter > 0)  {
        memberNode->timeOutCounter--;
        
        if (memberNode->pingCounter > 0) {
            memberNode->pingCounter--;
            
            if (memberNode->pingCounter == 0){
                // Check for outstanding pings
                // If outstanding ping
                //          - check for any joins or faileds
                //          - send indping with piggybacked joins and fails
                //          - add indping to indping table
                //          - delete the joins and fails that were piggybacked
            }
        }
        
    }
    
    if (memberNode->timeOutCounter <= 0) {
        memberNode->timeOutCounter = TIMEOUT;
        memberNode->pingCounter = TFAIL;
        toind = rand() % memberNode->memberList.size();
        while (memberNode->memberList[toind].getid() == *(int *)(memberNode->addr.addr)) {
            toind = rand() % memberNode->memberList.size();
        }

        *(int *)(&toaddr.addr)= (int) memberNode->memberList[toind].getid() ;
        *(short *)(&toaddr.addr[4]) = (short) memberNode->memberList[toind].getport();
        sendPING(&toaddr, memberNode->memberList);
        
        
        
        // Check for outstanding indpings
        // If outstanding indping
        //      - delete the member from the member table
        //      - add member to Failed list
        //      - send out ping to random
        //      - add ping to ping table
    }
    

	/*
	 * Your code goes here
	 */

    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
    memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: initFailedList
 *
 * DESCRIPTION: Initialize the failed list
 */
void MP1Node::initFailedList() {
    this->failedList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}

/**
 * FUNCTION NAME: getSenderInfo
 *
 * DESCRIPTION: Get the message type, peer address and heartbeat information.
 */
void MP1Node::getSenderInfo(char *data, MessageHdr *msgHdr, Address *addr, long *heartbeat, char **endptr){

    *endptr = (char *)data;
    
    memcpy(msgHdr, data, sizeof(int));
    *endptr +=  sizeof(int);
    memcpy(&(addr->addr), (char *) data + sizeof(int), sizeof(addr->addr));
    *endptr +=  sizeof(addr->addr);
    memcpy((long *) heartbeat, (char *)(data) + sizeof(int) + 1 + sizeof(addr->addr), sizeof(long));
    *endptr +=  1 + sizeof(long);

}

/**
 * FUNCTION NAME: createMLEFromValues
 *
 * DESCRIPTION: Create a MemberListEntry object with the address, heartbeat and timestamp supplied.
 *              Memory should already have been allocated for the MemberListEntry object.
 *
 */
void MP1Node::updateMLEFromValues(MemberListEntry *mle, Address *addr, long *heartbeat, long *timestamp){
    
    //memset(mle, 0, sizeof(MemberListEntry));
    
    mle->setid ((int)addr->addr[0]);
    mle->setport((short)addr->addr[4]);
    mle->setheartbeat(*heartbeat);
    mle->settimestamp(*timestamp);
    
    return;
}

/**
 * FUNCTION NAME: getValuesFromMLE
 *
 * DESCRIPTION: Get the address, heartbeat and timestamp from the MemberListEntry.
 *
 */
void MP1Node::getValuesFromMLE(MemberListEntry *mle, Address *addr, long *heartbeat, long *timestamp){

    *(int *)(addr->addr)= (int)mle->getid();
    *(short *)(&addr->addr[4]) = (short)mle->getport();
    *heartbeat = mle->getheartbeat();
    *timestamp = mle->gettimestamp();

    return;
}

/**
 * FUNCTION NAME: addMember
 *
 * DESCRIPTION: Add member to member list.
 *
 */
void MP1Node::addMember(MemberListEntry *peer) {
    MemberListEntry *mle;
    Address peeraddr;
    int i = 0;
    bool found = false;
    
    while (i < memberNode->memberList.size() && not(found)) {
        if (peer->getid() == memberNode->memberList[i].getid() &&
            peer->getport()== memberNode->memberList[i].getport()) {
            cout<<"Found a match in the list"<<endl;
            found = true;
        }
        i++;
    }
    
    if (found) {
        // Update existing member
        memberNode->memberList[i].setheartbeat(peer->getheartbeat());
        memberNode->memberList[i].settimestamp(memberNode->heartbeat);
    } else {
        // Add new member to the list
        mle = (MemberListEntry *) malloc (sizeof(MemberListEntry));
        mle->setid (peer->getid());
        mle->setport(peer->getport());
        mle->setheartbeat(peer->getheartbeat());
        mle->settimestamp(memberNode->heartbeat);
        memberNode->memberList.push_back(*mle);
        peeraddr.addr[0] = (int) peer->getid();
        peeraddr.addr[4] = (short) peer->getport();
        log->logNodeAdd(&(memberNode->addr), &peeraddr );
    }
    
    return;
}

/**
 * FUNCTION NAME: createMsgHeader
 *
 * DESCRIPTION: Create a message header for the current member.  Memory for the message should
 *              have already been allocated.
 *
 */
void MP1Node::createMessageHdr(MessageHdr *msg, MsgTypes msgtype, Address *addr, long *heartbeat, char **endptr){
    
    *endptr = (char *)msg; // endptr points to the current end of data
    
    msg->msgType = msgtype;
    *endptr +=  sizeof(int);
    memcpy((char *)(msg+1), addr->addr, sizeof(addr->addr));
    *endptr += sizeof(addr->addr) + 1;
    memcpy((char *)(msg+1) + 1 + sizeof(addr->addr), heartbeat, sizeof(long));
    *endptr += sizeof(long);
    
    return;
}

/**
 * FUNCTION NAME: sendJOINREP
 *
 * DESCRIPTION: Send JOINREP message response.  The message table is appended after the
 *              heartbeat.  So message structure is:
 *                  JOINREP
 *                  memberNode->addr.addr
 *                  memberNode->heartbeat
 *                  memberNode->memberList
 */
void MP1Node::sendJOINREP(Address *toaddr, std::vector<MemberListEntry> ml) {
    MessageHdr *msg;
    char *ptr;
    MemberListEntry *mle;
#ifdef DEBUGLOG
    static char s[1024];
#endif
    
    size_t msgsize = sizeof(MessageHdr) + sizeof(toaddr->addr) + sizeof(long) + 1
    + sizeof(MemberListEntry)*ml.size();
    msg = (MessageHdr *) malloc(msgsize * sizeof(char));
    
    // create JOINREP message: format of data is
    createMessageHdr(msg, JOINREP, &memberNode->addr, &memberNode->heartbeat, &ptr);
    for (int i = 0; i < ml.size(); i++) {
        mle = &ml[i];
        *ptr = (int) mle->getid();
        ptr += sizeof(int);
        *ptr = (short) mle->getport();
        ptr += sizeof(short);
        *ptr = (long) mle->getheartbeat();
        ptr += sizeof(long);
    }

    
    cout << "Sending JOINREP: " << memberNode->addr.addr <<" heartbeat: " << memberNode->heartbeat << endl;
#ifdef DEBUGLOG
    sprintf(s, "Sending join response...");
    log->LOG(&memberNode->addr, s);
#endif
    
    // send JOINREP message to new peer
    emulNet->ENsend(&memberNode->addr, toaddr, (char *)msg, msgsize);
    
    free(msg);
    
    return;
}

/**
 * FUNCTION NAME: sendPING
 *
 * DESCRIPTION: Send PING message.   The message structure is:
 *                  PING
 *                  memberNode->addr.addr
 *                  memberNode->heartbeat
 *                  FAILED
 *                  failedpeer->addr
 */
void MP1Node::sendPING(Address *toaddr, std::vector<MemberListEntry> ml) {
    MessageHdr *msg;
    char *ptr;
    MemberListEntry *mle;
#ifdef DEBUGLOG
    static char s[1024];
#endif
    
    size_t msgsize = sizeof(MessageHdr) + sizeof(toaddr->addr) + sizeof(long) + 1
    + sizeof(MemberListEntry)*ml.size();
    msg = (MessageHdr *) malloc(msgsize * sizeof(char));
    
    // create PING message: format of data is
    createMessageHdr(msg, PING, &memberNode->addr, &memberNode->heartbeat, &ptr);
    for (int i = 0; i < ml.size(); i++) {
        mle = &ml[i];
        *ptr = (int) mle->getid();
        ptr += sizeof(int);
        *ptr = (short) mle->getport();
        ptr += sizeof(short);
        *ptr = (long) mle->getheartbeat();
        ptr += sizeof(long);
    }
    
    
    cout << "Sending PING: " << memberNode->addr.addr <<" heartbeat: " << memberNode->heartbeat << endl;
#ifdef DEBUGLOG
    sprintf(s, "Sending ping...");
    log->LOG(&memberNode->addr, s);
#endif
    
    // send JOINREP message to new peer
    emulNet->ENsend(&memberNode->addr, toaddr, (char *)msg, msgsize);
    
    free(msg);
    
    return;
}
