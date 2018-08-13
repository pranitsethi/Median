 /* All rights reserved.
 *  // TODO: COMMENTS/EXPLAINATION
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

using namespace std;

// OVERFLOWS
// lets declare two typedefs to handle overflows
// if this program is used for very large sized arrays and nodes then 
// switch out the mysize_t in favor of a ulong64_t (long long) or __u128__. 
typedef unsigned int mysize_t;
typedef signed int myssize_t;   // also need a signed int (at least for elements in arrays)

class InterNodeFlushManager;
class DistributedMedian;
class DistributedArrayManager;

/** 
 * A class used for input validation. 
 * 
*/

class Validation { 
  // class used to validate the inputs

public:
   bool static validateInputs(mysize_t N, mysize_t elementCount);
};

/* 
 * A class that does the median finding
 * Median-of-three:
 * median-of-three with a cutoff for small subfiles/arrays 
 * can improve the running time of quicksort/select by 20 to 25 percent
 */

class DistributedMedian { 

    DistributedArrayManager* dam;
    InterNodeFlushManager*   infm;

    const mysize_t cutoff = 3; // switches to median of medians
    const mysize_t diffIndex = 3; // switches to median of medians
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
     void     compexchsimple(myssize_t element1, myssize_t element2);
     void     compexch(mysize_t index1, mysize_t index2);
     void     insertionSortDistributed(mysize_t left_index, mysize_t right_index); // indexes
     void     medianInsertionSortDistributed();
     void     insertionSortSimple();
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


public: 

    DistributedArrayManager(mysize_t Nodes, mysize_t numElements);
    void      setElement(mysize_t index, myssize_t element);  // goes through the cache
    void      setElementFlush(mysize_t index, myssize_t element); // goes direct (ONLY called by cache flush or input-init) 
    myssize_t getElement(mysize_t index);
    bool      checkCache(mysize_t index);
    void      printAllArrays();
    mysize_t  getNumNodes() { return numNodes; }
    mysize_t  getNode(mysize_t index) { return index/numElementsPerArray; }
    mysize_t  getTotalElements() { return totalElements; }
    myssize_t getElementAtIndex(mysize_t index) { assert (index >= 0 && index < totalElements); return getElement(index); } // TODO: use batchManager to read a bunch of entries at once (read ahead):  especially for partition() 
    double   findMedian(); 


    friend class InterNodeFlushManager;
    friend class DistributedMedian;

};

/** 
 * A class to manage communication between the nodes. 
 * tries to batch the messages to reduce the messages.
 * 
 */

class InterNodeFlushManager { 

  // this object manaages communication amidst the nodes
  // however in a batched fashion so to minmize the need to 
  // hit the wire

  // each node has one of these to accrue changes to be sent to every other node (if required)

class BatchInfo { 

public:
    BatchInfo* next; 
    mysize_t Node; 
    std::vector<std::pair<mysize_t, myssize_t>> ev; 
    mysize_t getSize() { ev.size(); }
    BatchInfo(mysize_t n): Node(n), next(NULL) {}   // next == NULL for the simpler approach
  };

   DistributedArrayManager* dam;
   BatchInfo** BatchNodes;

public:
   InterNodeFlushManager(DistributedArrayManager* d);
   void flush(); // method which sends the information to nodes in batched form
   void swapValues(mysize_t src_index, mysize_t dest_index); // async: queues it up and passes a batch message eventually
// void queuePair(mysize_t index, myssize_t element) { BatchNodes[dam->getNode(index)]->ev.push_back(std::make_pair(index, element)); }
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
    const static mysize_t N = 10; // tweak: how much memory is availiable?

public: 
     
      ReadAheadCache() {}


};



// TODO
// qS (with 3): enhancement
// use Double when total array size is even (n + n+1/2) *
// MoM *
// policy: cut over from qS to Mom *
// git: clean up * 
// indentation
// insertion sort *
// read ahead cache (getElementAtIndex(x)): constraint (memory on controller node: memory threshold)
// -- lookup flush manager cache *
// -- update index if it exists (or for now just append) *
// statisic generator (InterNode messages) 
// TESTING: auto array element generator
