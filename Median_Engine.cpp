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
      distrMedian = new DistributedMedian(this, infm);
}

void
DistributedArrayManager::printAllArrays() 
{

     for (mysize_t n = 0; n < numNodes; n++) {
	printf("\nelements of %d array\n", n + 1);
	for (mysize_t j = 0; j < numElementsPerArray; ++j) {
	  printf("%d  ", getElement(n, j));
	}
     }
	printf("\n\n");
}


InterNodeFlushManager::InterNodeFlushManager(DistributedArrayManager* d) : dam(d) 

{

     mysize_t nodes = dam->getNumNodes();
    printf("INFM: got %d nodes\n", nodes); 
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

void
InterNodeFlushManager::queuePair(mysize_t index, myssize_t element)
{

   // queue up the element to swap
   // flush will get to all the elements destined for a node

    mysize_t node = dam->getNode(index);
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
      //for (it = BatchNodes[n]->ev.begin(); it != BatchNodes[n]->ev.end(); ++it) 
      while (BatchNodes[n]->ev.size())
       {
        it = BatchNodes[n]->ev.begin();
        // this encapsulates a message for all data and sends it to node 'n'
        // a worker recieves the message and then swaps in the new values (local)
        printf("flush found index %d, element %d, node %d, szv %lu\n", it->first, it->second, n, BatchNodes[n]->ev.size()); 
        dam->setElement(it->first, it->second);
        BatchNodes[n]->ev.erase(BatchNodes[n]->ev.begin());
       }
       assert(BatchNodes[n]->ev.size() == 0);
   }
} 

void
DistributedMedian::exch(mysize_t index1, mysize_t index2) 
{
   // exchange the values at index1 & index2
   // However, its an async batched operation 
   // via the internodeflushmanager
   infm->swapValues(index1, index2);
}

mysize_t 
DistributedMedian::partition(mysize_t left, mysize_t right) 
{
   // classic parition
   // randomly chooses to partition with the last element
   mysize_t i = left - 1, j = right; myssize_t v = dam->getElementAtIndex(right);
   printf("partition i %d, j %d element %d\n", i,j, v); 
   for(;;)
    {
 
     while (dam->getElementAtIndex(++i) < v) ; 
     while (v < dam->getElementAtIndex(--j)) if (j == left) break;
     if (i >= j) break;
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
    // correctness: even or odd sized array TODO
    // random selection of pivot (last element or something else)
    // iterative to eliminate tail recursion

    mysize_t left = 0; 
    mysize_t right = dam->getTotalElements() - 1;
    mysize_t median = (right + 1)/ 2;
    mysize_t pivotIndex = right;
    printf("enter quick select r %d, m %d, pi %d\n", right, median, pivotIndex); 

    while (right > left) {
    
        pivotIndex = partition(left, right);
        if (pivotIndex >= median) right = pivotIndex - 1; 
        if (pivotIndex <= median) left  = pivotIndex + 1; 
    }

    if (dam->getTotalElements() % 2 == 0) { 
       int i = dam->getElementAtIndex(pivotIndex - 1);
       int k = dam->getElementAtIndex(pivotIndex );
       double j = (i + k)/2.;
       printf("even elements pi %d, i %d, k %d, median %lf\n", pivotIndex,i, k, j); //TODO: REMOVE
       return j;
    } else { 
        return dam->getElementAtIndex(pivotIndex);
    } 
}

/*mysize_t 
DistributedMedian::quickSelect() 
{

    // correctness: even or odd sized array TODO
    // random selection of pivot (last element or something else)
    // iterative to eliminate tail recursion

    mysize_t left = 0; 
    mysize_t right = dam->getTotalElements() - 1;
    mysize_t median = (right + 1)/ 2;
    mysize_t pivotIndex = right;
    printf("enter quick select r %d, m %d, pi %d\n", right, median, pivotIndex); 
    
     //loop
     while (pivotIndex != median) {
     
         if (left == right)
             return dam->getElementAtIndex(left);
         //pivotIndex = left + floor(rand() % (right - left + 1));     // randomly select pivotIndex between left and right (TODO: text)
         pivotIndex = partition(left, right);
         if (median == pivotIndex)
             return dam->getElementAtIndex(pivotIndex);
         else if (median < pivotIndex)
             right = pivotIndex - 1;
         else
             left = pivotIndex + 1;
     }

     if (dam->getTotalElements() % 2 == 0) { 
        return (dam->getElementAtIndex(pivotIndex - 1) + dam->getElementAtIndex(pivotIndex)) / 2; 
     } else { 
         return dam->getElementAtIndex(pivotIndex);
     } 
}*/

           
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
    

     printf("lets enter elements of each array\n");
     for (mysize_t n = 0; n < numNodes; n++) {
	printf("lets enter elements of %d array, space separated\n", n + 1);
        mysize_t element;
	for (mysize_t j = 0; j < numElements; ++j) {
            scanf("%d", &element);
            Dam->setElement(n, j, element);
            //Dam->setElement(j, element);
	}
     }

     Dam->printAllArrays();



     printf("\nWelcome to the median engine\n"); 

     double median = Dam->findMedian();

     printf("\n median found is %lf\n", median); 

     Dam->printAllArrays();


     return 0;

}
    
