/* // TODO: COMMENTS
 *
 *
*/


#include<Median_Engine.h> 



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
}

void
DistributedArrayManager::printAllArrays() 
{

     for (mysize_t n = 0; n < numNodes; n++) {
	printf("\nelements of %d array\n", n + 1);
	for (mysize_t j = 0; j < totalElements/numNodes; ++j) {
	  printf("%d  ", getElement(n, j));
	}
     }
}


InterNodeFlushManager::InterNodeFlushManager(DistributedArrayManager* dam) 

{

     mysize_t nodes = dam->getNumNodes();
     //BatchNodes = new BatchInfo**[nodes]; // Time permitting; more elaborate per node BatchNode
     BatchNodes = new BatchInfo*[nodes];
     if (!BatchNodes) { 
        // TODO: deal with the error
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

    mysize_t element1 = dam->getElementAtIndex(index1); // takes advantage of read ahead
    mysize_t element2 = dam->getElementAtIndex(index2);

    queuePair(index2, element1); // indexes and values swapped
    queuePair(index1, element2);

}

void
InterNodeFlushManager::flush()
{

   // walks the BatchNodes data structure and sends maximum of one message
   // per node to send new values. If there are no new values for a node, then
   // no messages are sent to that node.





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
DistributedMedian::partition() 
{
   // classic parition
   mysize_t i = left - 1, j = right; myssize_t v = dam->getElementAtIndex(right);
   for(;;)
    { 
     while (dam->getElementAtIndex(++i) < v) ; 
     while (v < dam->getElementAtIndex(--j)) if (j == left) break;
     if (i >= j) break;
        //exch(dam->elementAtIndex(i), dam->elementAtIndex(j)); 
        exch(i, j); 
    }
     //exch(dam->elementAtIndex(i), dam->elementAtIndex(right)); 
     exch(i, right); 

   infm->flush(); // TODO: can this be further delayed? (upto one message to each node possible here)

   return i; // pivot
}
 
mysize_t 
DistributedMedian::quickSelect() 
{

    // correctness: even or odd sized array TODO
    // random selection of pivot
    
     /*loop
         if left = right
             return list[left]
         pivotIndex := left + floor(rand() % (right - left + 1));     // randomly select pivotIndex between left and right (TODO: text)
         pivotIndex := partition(list, left, right, pivotIndex)
         if k = pivotIndex
             return list[k]
         else if k < pivotIndex
             right := pivotIndex - 1
         else
             left := pivotIndex + 1
     */
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
     // enter elements mysize_to arrays

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
            //Dam->setElement(Node, index, element));
            Dam->setElement(n, j, element);
	}
     }

     Dam->printAllArrays();



     printf("\nWelcome to the median engine\n"); 

     // quickSelect; median of medians if necessary


     return 0;

}
    
