#ifndef bufIdAddOrderSet_h
#define bufIdAddOrderSet_h

#include "bufferId.h"

#include <unordered_map>
#include <list>
#include <mutex>

// the goal of this is to make a set equivalent of python's ordered dict.  Specifically elements can
// be added to or deleted from the set as well as checked if they exist in the set.  But additionally
// they can be iterated over in order of insertion.

// this is traditionally implemented by adding linked list pointers to an associative array.  I don't
// feel like rolling my own so I'm just going to use two stl containers and link things together.

// I'm not going to pretend that this is remotely optimal.

#include <stdio.h>

typedef std::list<BufferId>::iterator blIt;

class BufIdSet {
    std::list<BufferId> bIdList;
    std::unordered_map<BufferId, blIt, BufferIdHash, BufferIdEqual> bIdMap;
    std::mutex m;

public:
    void add(BufferId bId) {
	//printf("bufIdSet add ");
	//bId.print();
	//printf("\n");
	std::lock_guard<std::mutex> lock(m);
	blIt bRef = bIdList.insert(bIdList.begin(), bId);
	bIdMap[bId] = bRef;
    }

    // since deleting buffers and pushing buffers to disk can compete with each other
    // we need to hold the bufDirMutex while del()ing and pop()ing the bId's
    
    void del(BufferId bId) {
	//printf("bufIdSet del ");
	//bId.print();
	//printf("\n");
	std::lock_guard<std::mutex> lock(m);
	blIt bRef = bIdMap[bId];
	bIdMap.erase(bId);
	bIdList.erase(bRef);
    }

    BufferId pop() {
	std::lock_guard<std::mutex> lock(m);
	if (0 == bIdList.size())
	    return BufferId("");
	BufferId bId = bIdList.back();
	//printf("bufIdSet pop ");
	//bId.print();
	//printf("\n");
	bIdList.pop_back();
	bIdMap.erase(bId);
	
	return bId;
    }
};

#endif
