 /*  // TODO: COMMENTS/EXPLAINATION
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <hiredis/hiredis.h>
#include <map>
#include <list>
#include <unistd.h>
#include <iostream>
#include <assert.h>
#include <math.h>
#include <algorithm>

using namespace std;

// OVERFLOWS
// lets declare two typedefs to handle overflows
// if this program is used for very large sized arrays and nodes then 
// switch out the mysize_t in favor of a ulong64_t (long long) or __u128__. 
typedef unsigned int mysize_t;
typedef signed int myssize_t;   // all elements must be of type myssize_t (if they are not, its a bug!)

#define STATS_ENABLED
#define VALIDATION_ENABLED

class InterNodeFlushManager;
class DistributedMedian;
class DistributedArrayManager;
class ReadAheadCache;


/** 
 * A class used for input validation. 
 * (can't be disabled-- doesn't affect runtime) 
*/
class Validation { 

public:
   bool static validateInputs(mysize_t N, mysize_t elementCount);
};

/* 
 * A class that does distributed median calculation.
 * It uses the distributed Array Manager to access the data.
 * algorithm:
 * median finding will use quickSelect (see README) unless
 * its in a degenerate case. If so, then median-of-medians.
 */

class DistributedMedian { 

    DistributedArrayManager* dam;
    InterNodeFlushManager*   infm;

    const mysize_t cutoff = 3; // switches to median of medians
    const mysize_t diffIndex = 3; // && switches to median of medians
    mysize_t  lastPivotIndex;
    mysize_t  progressCount;

public: 
 
     DistributedMedian(DistributedArrayManager* d, InterNodeFlushManager* fm): dam(d), infm(fm), progressCount(0) { }
     double   quickSelect(); 
     double   medianOfMedians(vector<mysize_t>&, mysize_t k, mysize_t left, mysize_t right); 
     double   medianOfMediansFinal(vector<mysize_t>&, mysize_t k, mysize_t left, mysize_t right, bool mom = false); 
     mysize_t partition(mysize_t left, mysize_t right, mysize_t pivotIndex=0);
     bool     checkQuickSelectProgress(mysize_t pivotIndex); // TODO: needs more thought
     void     exch(mysize_t index1, mysize_t index2);
     void     compexch(mysize_t index1, mysize_t index2);
     void     insertionSortDistributed(mysize_t left_index, mysize_t right_index); // hint: args are indexes
     double   checkForMedian(mysize_t index);
     bool     didQSComplete() { return progressCount < cutoff; } 
};


/** 
 * A class responsible for hiding the distributed nature of the data
 * in the arrays and to make the distributed arrays look like one big array. 
 * 
 */
class DistributedArrayManager { 

     mysize_t  numNodes;
     myssize_t** nodesArray; // map to all arrays
     mysize_t  totalElements; // accross all arrays
     mysize_t  numElementsPerArray; // easy to save than calculate each time (totalElements/numNodes) 
     DistributedMedian* distributedMedian;
     InterNodeFlushManager* infm;
     ReadAheadCache*  rac;
#ifdef STATS_ENABLED
    mysize_t  InterNodeMessages;
#endif


public: 

    DistributedArrayManager(mysize_t Nodes, mysize_t numElements);
    void      setElement(mysize_t index, myssize_t element);  // goes through the cache
    void      setElementFlush(mysize_t index, myssize_t element); // goes direct (ONLY called by cache flush or input-init) 
    myssize_t getElement(mysize_t index);
    myssize_t getElementNoCache(mysize_t index);
    mysize_t  getRelativeIndex(mysize_t index) { return index % numElementsPerArray; }
    bool      checkCache(mysize_t index);
    void      printAllArrays();
    mysize_t  getNumNodes() { return numNodes; }
    mysize_t  getNode(mysize_t index) { return index/numElementsPerArray; }
    mysize_t  getTotalElements() { return totalElements; }
    mysize_t  getElementsPerArray() { return numElementsPerArray; }
    myssize_t getElementAtIndex(mysize_t index) { assert (index >= 0 && index < totalElements); return getElement(index); }
    double    findMedian();
#ifdef STATS_ENABLED
    bool      momUsed() { return !distributedMedian->didQSComplete(); }
    bool      getInterNodeMessages() { return InterNodeMessages; }
#endif

#ifdef VALIDATION_ENABLED
   double   validateMedian(); 
   vector<myssize_t> medianValidate;
#endif


    friend class InterNodeFlushManager;
    friend class DistributedMedian;

};

/** 
 * A class to manage communication between the nodes. 
 * Batches the elements to reduce the # of messages.
 * 
 */

class InterNodeFlushManager { 

  // this object manaages communication amidst the nodes
  // however in a batched fashion so to minmize the need to 
  // hit the wire

  // each node has one of these to accrue changes to be sent to every other node (if required)

 /* 
  * BatchInfo collects and manages the element information to be flushed by the flush manager 
  *
  */
  class BatchInfo { 

  public:
    BatchInfo* next; 
    mysize_t Node; 
    std::vector<std::pair<mysize_t, myssize_t>> ev; 
    mysize_t getSize() { return ev.size(); }
    BatchInfo(mysize_t n): next(NULL), Node(n) {}   // next == NULL for the simpler approach
  };

   DistributedArrayManager* dam;
   BatchInfo** BatchNodes;

public:
   InterNodeFlushManager(DistributedArrayManager* d);
   void flush(); // method which sends the information to nodes in batched form
   void swapValues(mysize_t src_index, mysize_t dest_index); // async: queues it up and passes a batch message eventually
   // Note: ReadAheadCache (queuePair to implement LRU eviction)
   void queuePair(mysize_t index, myssize_t element);
   bool checkCache(mysize_t index, myssize_t& element);

};


/*
 * A class to cache N elements from a node that is being worked on (say for partition)
 * This class would intercept a getElementAtIndex() call for instance and return the element
 * if present than make a trip to the node. If it does need to go out to the node, then it'll bring 
 * N elements to cache. We can implement a LRU cache eviction.
 * For cache and element coherency, the element must be removed (or updated) from here if it's swapped
 * or some other action and moved to the InterNodeFlushManager cache. Combining the two (ReadAheadCache and
 * InterNodeFlushManager) is a possiblity but it might lead to complications and complicated logic (for starters
 * would need a state bit to identify whether an element is dirty to be picked up by the flush manager)
 */

class ReadAheadCache {


   // memory threshold (LRU eviction policy)
   const mysize_t MEM_THRESH = 10; // tweak: how much memory is availiable?
   DistributedArrayManager* dam;
   InterNodeFlushManager* infm;

public: 
     
    ReadAheadCache(DistributedArrayManager* d, InterNodeFlushManager* inf): dam(d), infm(inf) {}
    void fetchElements(mysize_t index, bool forward, bool backward); // TODO: more on args
};



// TODO
// qS (with 3): enhancement
// indentation
// read ahead cache (getElementAtIndex(x)): constraint (memory on controller node: memory threshold)
// statisic generator (InterNode messages) 
// README
// printf 
// TODO
// diff negative bug
