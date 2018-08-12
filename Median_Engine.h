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
 * 
 */

class DistributedMedian { 

    DistributedArrayManager* dam;
    InterNodeFlushManager*   infm;
    //size_t left; 
    //size_t right;

public: 
 
     //DistributedMedian(DistributedArrayManager* d, InterNodeFlushManager* fm): dam(d), infm(fm), left(0){ right = dam->getTotalElements() - 1 ; }
     DistributedMedian(DistributedArrayManager* d, InterNodeFlushManager* fm): dam(d), infm(fm) { }
     double quickSelect(); 
     mysize_t medianOfMedians(); 
     mysize_t partition(mysize_t left, mysize_t right);
     void     exch(mysize_t index1, mysize_t index2); // TODO: route via batch manager
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
     DistributedMedian* distrMedian;
     InterNodeFlushManager* infm;


public: 

    DistributedArrayManager(mysize_t Nodes, mysize_t numElements);
    void      setElement(mysize_t index, myssize_t element) { myssize_t* nArray = nodesArray[getNode(index)]; mysize_t rindex = index - getNode(index) * numElementsPerArray; nArray[rindex] = element; }
    myssize_t getElement(mysize_t index) { myssize_t* nArray = nodesArray[getNode(index)]; mysize_t rindex = index - getNode(index) * numElementsPerArray; return nArray[rindex]; }
    void      printAllArrays();
    mysize_t  getElement(mysize_t node, mysize_t rindex) { myssize_t* nArray = nodesArray[node]; return nArray[rindex]; }
    mysize_t  setElement(mysize_t node, mysize_t rindex, myssize_t element) { myssize_t* nArray = nodesArray[node]; nArray[rindex] = element; }
    mysize_t  getNumNodes() { return numNodes; }
    mysize_t  getNode(mysize_t index) { return index/numElementsPerArray; }
    mysize_t  getTotalElements() { return totalElements; }
    myssize_t getElementAtIndex(mysize_t index) { return getElement(index); } // TODO: use batchManager to read a bunch of entries at once (read ahead):  especially for partition() 
    double   findMedian() { /* !inline TODO MoM */ return distrMedian->quickSelect(); }


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
//   void queuePair(mysize_t index, myssize_t element) { BatchNodes[dam->getNode(index)]->ev.push_back(std::make_pair(index, element)); }
   void queuePair(mysize_t index, myssize_t element);

};



// TODO
// qS (with 3)
// use Double when total array size is even (n + n+1/2)
// MoM 
// policy: cut over from qS to Mom
