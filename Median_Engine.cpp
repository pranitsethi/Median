/* // TODO: COMMENTS
 *
 *
*/


#include<Median_Engine.h> 

using namespace std;

bool 
Validation::validateInputs(mysize_t Nodes, mysize_t arraySize) 
{ 
      if (!Nodes) {

         printf("You have entered %d node\n", Nodes); 
         printf("We need at least 1 to continue; please try again\n");
         return false;

       } else if (Nodes == 1) { 

        printf("You have entered %d Node\n", Nodes); 

        } else { 

        printf("You have entered %d Nodes\n", Nodes); 
      }

      if (!arraySize) {

         printf("You have entered %d size array\n", arraySize); 
         printf("We need at least 1 element in each array to continue; please try again\n");
         return false;

       } else if (arraySize == 1) { 

        printf("You have entered each array with %d element\n", arraySize); 

       } else { 

        printf("You have entered each array with %d number of elements\n", arraySize); 
      }
      printf("\n"); 

      return true;

}

DistributedArrayManager::DistributedArrayManager(mysize_t Nodes, mysize_t numElements): numNodes(Nodes), totalElements(Nodes * numElements) 
{


        nodesArray = new myssize_t*[numNodes];
        if (!nodesArray) { 
           // TODO: deal with the alloc failure
        }

       for (mysize_t i = 0; i < Nodes; ++i) 
       {
           // create numNodes arrays 
          nodesArray[i] = new myssize_t[numElements]; 
          if (!nodesArray[i]) { 

           // TODO: deal with the alloc failure
          }
       }

      numElementsPerArray = numElements;
      infm = new InterNodeFlushManager(this);
      distributedMedian = new DistributedMedian(this, infm);
      rac = new ReadAheadCache(this, infm);

#ifdef VALIDATION_ENABLED
      // medianValidate = new myssize_t[Nodes*numElements]; //TODO: REMOVE
#endif

}

void
DistributedArrayManager::printAllArrays() 
{

     for (mysize_t n = 0; n < numNodes; n++) {
	printf("\nelements of %d array\n", n + 1);
	for (mysize_t j = 0; j < numElementsPerArray; ++j) {
	  printf("%d  ", getElement((n*numElementsPerArray) + j));
	}
     }
	printf("\n\n");
}

double
DistributedArrayManager::findMedian() 
{

    // finds the median either via quickSelect or median of medians

    double qs_median = distributedMedian->quickSelect();
    if (distributedMedian->didQSComplete() == true)
        return qs_median;

    // median of medians
    vector<mysize_t> dummyVector;
    mysize_t median = getTotalElements()/2; 
    return distributedMedian->medianOfMedians(dummyVector, median, 0, getTotalElements() - 1);
}

double
DistributedArrayManager::validateMedian()
{

   //sort(medianValidate, 0, totalElements - 1); 
   sort(medianValidate.begin(), medianValidate.end());
   if (totalElements % 2 == 0) { 
       int i = medianValidate[totalElements/2 - 1];
       int k = medianValidate[totalElements/2];
       double j = (i + k)/2.;
       return j;
    } 
    return medianValidate[totalElements/2];

}

void 
DistributedArrayManager::setElementFlush(mysize_t index, myssize_t element) 
{ 
   myssize_t* nArray = nodesArray[getNode(index)]; 
   mysize_t rindex = index - getNode(index) * numElementsPerArray; 
   nArray[rindex] = element; 
}

void
DistributedArrayManager::setElement(mysize_t index, myssize_t element)
{ 
   // ONLY sets the cache
   infm->queuePair(index, element);
}

myssize_t 
DistributedArrayManager::getElement(mysize_t index) 
{

   // always looks into the cache first
   // if found, returns from the cache (saves a node round trip -- 
   // read from the node -- will be a bulk read ahead)
   myssize_t element;
   if (infm->checkCache(index, element)) { 
      // found in the cache
     return element;
    }

   // TODO: ReadAheadCache->fetchElements(bool forward, bool backward); (uno internode message)
   // Now the element and its neighbours are in the cache (these elements are in clean state)
   // clean: don't need to be flushed back to the node (can be discarded by LRU)
   // get it from the cache now

   myssize_t* nArray = nodesArray[getNode(index)]; 
   mysize_t rindex = index - getNode(index) * numElementsPerArray; 
   return nArray[rindex];
}

myssize_t 
DistributedArrayManager::getElementNoCache(mysize_t index) 
{

   // used by ReadAheadCache->fetchElements(bool forward, bool backward); (uno internode message)
   // Now the element and its neighbours are in the cache (these elements are in clean state)
   // clean: don't need to be flushed back to the node (can be discarded by LRU)
   // get it from the cache now

#ifdef STATS_ENABLED
   ++InterNodeMessages;
#endif 

   myssize_t* nArray = nodesArray[getNode(index)]; 
   mysize_t rindex = index - getNode(index) * numElementsPerArray; 
   return nArray[rindex];
}

InterNodeFlushManager::InterNodeFlushManager(DistributedArrayManager* d) : dam(d) 
{

     mysize_t nodes = dam->getNumNodes();
     //BatchNodes = new BatchInfo**[nodes]; // Time permitting; more elaborate per node BatchNode
     BatchNodes = new BatchInfo*[nodes];
     if (!BatchNodes) {
        // TODO: deal with the error
     }

     for (mysize_t i = 0; i < nodes; ++i) { 

         BatchNodes[i] = new BatchInfo(i);
         if (!BatchNodes[i]) {
          // TODO: deal with the error
         }
     }

     // commented out: this would be even a more elaborate approach where there would be a 
     // InterNodeFlushManager per node with its own BatchNode data structure and parallel computing.
     // in the interest of time, might postpone implementation.
     /*for (mysize_t i = 0; i < nodes; ++i) { 

         BatchNodes[i] = new BatchInfo*[nodes];
         if (!BatchNodes[i]) { 
          // TODO: deal with the error
         }
     }*/
}

void
InterNodeFlushManager::swapValues(mysize_t index1, mysize_t index2)
{ 
   
    // In light of minimal inter node exchanges, INFM (Inter Node Flush Manager) queues up the values 
    // and flushes or exchanges the values between nodes in a batched manner. Saves many round trips.

    // goal: take value from index1 (say node1) and take value from index2 (say node2) and send them to the
    // other node with new index (async distributed swap).
    if (index1 == index2) { 
        printf("swapValues: Not queuing index1 %d, index2 %d - they are same\n", index1, index2); 
        return;
    }
    printf("swapValues: index1 %d, index2 %d\n", index1, index2); 

    mysize_t element1 = dam->getElementAtIndex(index1); // takes advantage of read ahead
    mysize_t element2 = dam->getElementAtIndex(index2);
    printf("swapValues: got values element1 %d, element2 %d\n", element1, element2); 

    queuePair(index2, element1); // indexes and values swapped
    queuePair(index1, element2);

}

bool
InterNodeFlushManager::checkCache(mysize_t index, myssize_t& element)
{
    mysize_t node = dam->getNode(index);
    vector<std::pair<mysize_t, myssize_t>>::iterator it;
    for (it = BatchNodes[node]->ev.begin(); it != BatchNodes[node]->ev.end(); ++it) {

           if (it->first == index) { 
             // found the pair in the list
             // there can only be one such pair for that index in the list
              printf("queuePair: check index %d, element %d, node %d, szv %lu\n", it->first, it->second, node, BatchNodes[node]->ev.size()); 
             element = it->second;
             return true;
            }
    }
   return false;
}

void
InterNodeFlushManager::queuePair(mysize_t index, myssize_t element)
{
   
   // TODO Note: ReadAheadCache (queuePair to implement LRU eviction)
   // might need to internally call flush()i -- if elements are dirty?


   // queue up the element to swap
   // flush will get to all the elements destined for a node

    mysize_t node = dam->getNode(index);
    bool found = false; 
    vector<std::pair<mysize_t, myssize_t>>::iterator it;
    for (it = BatchNodes[node]->ev.begin(); it != BatchNodes[node]->ev.end(); ++it) {

           if (it->first == index) { 
             // found the pair in the list; just update it (saves memory)
              printf("queuePair: found index %d, element %d, node %d, szv %lu\n", it->first, it->second, node, BatchNodes[node]->ev.size()); 
              printf("queuePair: Updated to index %d, element %d, node %d, szv %lu\n", index, element, node, BatchNodes[node]->ev.size()); 
             it->second = element; 
             found = true;
             break;
            }
    }

    if (!found)
        BatchNodes[node]->ev.push_back(std::make_pair(index, element));

    
    printf("queuePair: index %d, element %d, node %d, szv %lu\n", index, element, node, BatchNodes[node]->ev.size()); 
}

void
InterNodeFlushManager::flush()
{

   // walks the BatchNodes data structure and sends  a maximum of one message
   // per node to send new values accross. If there are no new values for a node, then
   // no messages are sent to that node.

   int nodes = dam->getNumNodes();
   for(int n = 0; n < nodes; ++n) 
    {
      vector<std::pair<mysize_t, myssize_t>>::iterator it;
      while (BatchNodes[n]->ev.size())
       {
        it = BatchNodes[n]->ev.begin();
        // this encapsulates a message for all data and sends it to node 'n'
        // a worker recieves the message and then swaps in the new values (local)
        // TODO: would also update the readAheadCache eventually (or only that if under memory threshold)
        printf("flush found index %d, element %d, node %d, szv %lu\n", it->first, it->second, n, BatchNodes[n]->ev.size()); 
        dam->setElementFlush(it->first, it->second);
        BatchNodes[n]->ev.erase(BatchNodes[n]->ev.begin());
       }
       assert(BatchNodes[n]->ev.size() == 0);
   }
}

void
DistributedMedian::insertionSortDistributed(mysize_t left, mysize_t right)
{

    dam->printAllArrays();
    //mysize_t left = 0; 
    //mysize_t right = dam->getTotalElements() - 1;
    mysize_t i;
    for(i = right; i > left; i--) compexch(i-1, i);
    dam->printAllArrays();
    for(i = left + 2; i <= right; i++) {
        mysize_t j = i; myssize_t v = dam->getElementAtIndex(i); // low cost
        while (j > 0 && v < dam->getElementAtIndex(j-1))
        { dam->setElement(j, dam->getElementAtIndex(j-1)); j--; } 
       dam->setElement(j, v);
    dam->printAllArrays();
    }
    infm->flush();
    dam->printAllArrays();
}

bool
DistributedMedian::checkQuickSelectProgress(mysize_t pivotIndex)
{
    
     mysize_t diff = pivotIndex - lastPivotIndex > 0 ? pivotIndex - lastPivotIndex : 
                   lastPivotIndex - pivotIndex;

//    if (abs(int(pivotIndex - lastPivotIndex)) < diffIndex) { // TODO: FIX
      if (diff < diffIndex) { // TODO: FIX
       ++progressCount;
       if (progressCount > cutoff) {
          printf("quick select not making sufficient progress, pc %d\n", progressCount); 
          printf("switching over to median of medians\n"); 
          return true;
       }
    }
    printf("diff %f, di %d, lp %d, pi %d\n", fabs(pivotIndex - lastPivotIndex), diffIndex, lastPivotIndex, pivotIndex);  // TODO: REMOVE
    lastPivotIndex = pivotIndex;
    return false; // do not switch to median of medians

}

void
DistributedMedian::exch(mysize_t index1, mysize_t index2) 
{
   // exchange the values at index1 & index2
   // However, its an async batched operation 
   // via the internodeflushmanager
   infm->swapValues(index1, index2);
}

void
DistributedMedian::compexch(mysize_t index1, mysize_t index2)
{

    if (dam->getElementAtIndex(index2) < dam->getElementAtIndex(index1))
        exch(index1, index2);

}

mysize_t 
DistributedMedian::partition(mysize_t left, mysize_t right, mysize_t pivotIndex) 
{
   // classic parition
   mysize_t i = left - 1, j = right; 
   myssize_t v;

   if (pivotIndex) {
      // median of medians sends in a pivotIndex
      v = dam->getElementAtIndex(pivotIndex);
    } else {
   // randomly chooses to partition with the last element
      v = dam->getElementAtIndex(right);
   }

   printf("partition i %d, j %d element %d\n", i,j, v); 
   for(;;)
    {
 
     while (dam->getElementAtIndex(++i) < v) ; 
     while (j > 0 && v < dam->getElementAtIndex(--j)) if (j == left) break;
     if (i >= j) break;
     else if(dam->getElementAtIndex(i) == dam->getElementAtIndex(j)) {++i; continue;} // duplicate elements
     exch(i, j); 
    }
   exch(i, right);

   infm->flush(); // TODO: can this be further delayed? (upto one message to each node possible here)

   printf("returning pivot %d\n", i); 
   return i; // pivot
}


double
DistributedMedian::quickSelect() 
{
    // random selection of pivot (last element or something else)
    // iterative to eliminate tail recursion

   /* TODO (use median-of-three)
   * Median-of-three:
   * median-of-three with a cutoff for small subfiles/arrays
   * can improve the running time of quicksort/select by 20 to 25 percent
   */

    mysize_t left = 0; 
    mysize_t right = dam->getTotalElements() - 1;
    mysize_t median = (right + 1)/ 2;
    mysize_t pivotIndex = right;
    printf("enter quick select r %d, m %d, pi %d\n", right, median, pivotIndex); //TODO: REMOVE

    while (right > left) {
    
        pivotIndex = partition(left, right);
        if (checkQuickSelectProgress(pivotIndex)) // true returns implies not good progress (possibly O(n^2))
           return 0; // this value is meaningless
        printf("partition pivotIndex %d\n",pivotIndex); //TODO: REMOVE
        if (pivotIndex >= median) right = pivotIndex - 1; 
        if (pivotIndex <= median) left  = pivotIndex + 1; 
    }

   return checkForMedian(pivotIndex);
}

double 
DistributedMedian::checkForMedian(mysize_t pivotIndex)
{

    if (dam->getTotalElements() % 2 == 0) { 
       int i = dam->getElementAtIndex(pivotIndex - 1);
       int k = dam->getElementAtIndex(pivotIndex );
       double j = (i + k)/2.;
       printf("even elements pi %d, i %d, k %d, median %lf\n", pivotIndex,i, k, j); //TODO: REMOVE
       return j;
    } 
    return dam->getElementAtIndex(pivotIndex);
}



double
DistributedMedian::medianOfMedians(vector<mysize_t>& vec, mysize_t k, mysize_t start, mysize_t end)
{
  // if the data set is small, sort it and return the median
   if(end - start < 10) {
      insertionSortDistributed(start, end);
      return checkForMedian(k);
   }

   return medianOfMediansFinal(vec, k, start, end);

}

double
DistributedMedian::medianOfMediansFinal(vector<mysize_t>& vec, mysize_t k, mysize_t start, mysize_t end, bool mom)
{
   // recursive method (watch out for the recursive depth here)
   // don't blow the stack! (add bounds check (check stack size on stacking every new
   // frame on the stack) -- TODO: get stack size).

    printf("enter MoM l, %d, r %d, m %d\n", start, end, k);  // TODO: REMOVE

   // base case
   if(end - start < 10) {
      insertionSortDistributed(start, end);
      printf("GOT KMEDIAN MoM %d\n", k);  // TODO: REMOVE
      if (mom) 
        return vec.at(k);
      else 
        return checkForMedian(k); //dam->getElementAtIndex(k);
    }
    
    vector<mysize_t> medians;
    /* sort every consecutive 5 */
    for(mysize_t i = start; i < end; i+=5){
        if(end - i < 10){
            insertionSortDistributed(i, end);
            medians.push_back(dam->getElementAtIndex((i+end)/2));
        }
        else{
            insertionSortDistributed(i, i+5);

            dam->printAllArrays(); // TODO: REMOVE
            medians.push_back(dam->getElementAtIndex(i+2));
        }
    }
    
    mysize_t median = medianOfMediansFinal(medians, medians.size()/2, 0, medians.size(), true);
    
    printf("GOT MEDIAN MoM %d\n", median);  // TODO: REMOVE
    dam->printAllArrays(); // TODO: REMOVE
    /* use the median to pivot around */
    mysize_t piv = partition(start, end, median);
    mysize_t length = piv - start + 1;
    
    if(k < length) {
        return medianOfMediansFinal(vec, k, start, piv);
    }
    else if(k > length){
        return medianOfMediansFinal(vec, k-length, piv+1, end);
    }
    else {
        return checkForMedian(k); //dam->getElementAtIndex(k);
     }
}

void
ReadAheadCache::fetchElements(mysize_t index, bool forward, bool back) 
{ 
   // this is especially useful for something like partition()
   // or insertionsort or the like. They look at elements sequentially
   // and the readAhead can bring N elements at a time from a node
   // and warm up the cache.
   mysize_t rindex = dam->getRelativeIndex(index);
   mysize_t elArray = dam->getElementsPerArray();
   mysize_t forward_index = (rindex + MEM_THRESH > elArray)? elArray - rindex : rindex + MEM_THRESH;
   mysize_t back_index = (rindex - MEM_THRESH < 0)? rindex : rindex - MEM_THRESH;

   if (forward) { 

      // probably in partition where both side elements are needed
      // TODO: heuristics
      for (mysize_t i = 0; i < forward_index; ++i) { 
          infm->queuePair(index + i , dam->getElementNoCache(index + i ));
      } 

    }

   if (back) { 
      for (mysize_t i = rindex; i < back_index; --i) { 
          infm->queuePair(index - 1, dam->getElementNoCache(index - i ));
      }
   }
}
           
int main(int argc, char **argv) 
{

     mysize_t numNodes, numElements;
     printf("please enter number of nodes\n"); 
     scanf("%d", &numNodes);
     printf("please enter number of elements or size (all arrays will be same size)\n"); 
     scanf("%d", &numElements);
     if (!Validation::validateInputs(numNodes, numElements)) 
        return -1;

     // validation test passed 
     // create a DAM (Distributed Array Manager) and 
     // enter elements into N arrays

     DistributedArrayManager* Dam = new DistributedArrayManager(numNodes, numElements);
     if(!Dam) { 

       // memory allocation failed
       printf("Unable to allocate DistributedArrayManager\n");
       return -1; 

     } 
   
     char ch;   
     printf("There are two options to enter elements: manually or autoGenerate (arrays will be printed to stdout)\n");
     printf("Would you like to enter elements of each array (Y/N)?\n");
     scanf(" %c", &ch);

     if (ch == 'y' || ch == 'Y' ) {

       printf("lets enter elements of each array\n");
       for (mysize_t n = 0; n < numNodes; n++) {
	 printf("lets enter elements of %d array, space separated\n", n + 1);
         mysize_t element;
	 for (mysize_t j = 0; j < numElements; ++j) {
            scanf("%d", &element);
            Dam->setElementFlush( (n * numElements) + j, element);
#ifdef VALIDATION_ENABLED
         //Dam->medianValidate[vindex++] = element;
         Dam->medianValidate.push_back(element);
#endif
	}
       }
     } else { 
       printf("autoGenerate will enter elements of each array\n");
       for (mysize_t n = 0; n < numNodes; n++) {
         mysize_t element;
	 for (mysize_t j = 0; j < numElements; ++j) {
            element = (17*rand() + 23) % 100;  // arbitrary: could be anything (including negative #s)
            Dam->setElementFlush( (n * numElements) + j, element);
#ifdef VALIDATION_ENABLED
            //Dam->medianValidate[vindex++] = element;
            Dam->medianValidate.push_back(element);
#endif
	}
       }
     }


     Dam->printAllArrays();



     printf("\nWelcome to the median engine\n"); 

     double median = Dam->findMedian();

     printf("\n median found is %lf\n", median);

#ifdef VALIDATION_ENABLED
     double vmedian =  Dam->validateMedian();
     printf("\n Validated Median found is %f\n", vmedian); 
#endif

#ifdef STATS_ENABLED
     printf("\n STATS\n"); 
     printf("\n InterNode messages passed (TODO) %d\n", Dam->getInterNodeMessages()); 
     printf("\n Was Median of Medians used = %d\n", Dam->momUsed()); 
     //printf("\n run time %d\n", dam->runTime());  // TODO
#endif

     Dam->printAllArrays();


     return 0;

}
    
