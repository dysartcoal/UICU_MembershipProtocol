/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class.
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "EmulNet.h"
#include "Queue.h"

/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 5
#define TIMEOUT 15

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Message Types
 */
enum MsgTypes{
    JOINREQ,
    JOINREP,
    PING,
    INDPING,
    PINGREP,
    INDPINGREP,
    JOINED,
    FAILED,
    DUMMYLASTMSGTYPE
};

/**
 * STRUCT NAME: MessageHdr
 *
 * DESCRIPTION: Header and content of a message
 */
typedef struct MessageHdr {
	enum MsgTypes msgType;
}MessageHdr;

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for failure detection
 */
class MP1Node {
private:
	EmulNet *emulNet;
	Log *log;
	Params *par;
	Member *memberNode;
	char NULLADDR[6];
    Address ping;
    Address indping;
    // Membership table
    vector<Address> failedList;

public:
	MP1Node(Member *, Params *, EmulNet *, Log *, Address *);
	Member * getMemberNode() {
		return memberNode;
	}
	int recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);
	void nodeStart(char *servaddrstr, short serverport);
	int initThisNode(Address *joinaddr);
	int introduceSelfToGroup(Address *joinAddress);
	int finishUpThisNode();
	void nodeLoop();
	void checkMessages();
	bool recvCallBack(void *env, char *data, int size);
	void nodeLoopOps();
	int isNullAddress(Address *addr);
	Address getJoinAddress();
	void initMemberListTable(Member *memberNode);
    void initFailedList();
	void printAddress(Address *addr);
	virtual ~MP1Node();
    void getSenderInfo(char *data, MessageHdr *msgHdr, Address *addr, long *heartbeat, char **endptr);
    int updateMLEFromValues(MemberListEntry *mle, Address *addr, long *heartbeat, long *timestamp);
    void getValuesFromMLE(MemberListEntry *mle, Address *addr, long *heartbeat, long *timestamp);
    int addMember(MemberListEntry *peer);
    int createMessageHdr(MessageHdr *msg, MsgTypes msgtype, Address *addr, long *heartbeat, char **endptr);
    int sendJOINREP(Address *toaddr, std::vector<MemberListEntry> ml);
};

#endif /* _MP1NODE_H_ */
