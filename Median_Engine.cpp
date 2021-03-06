/*  Median finder: 
 *  Given a set of (simulated) distributed nodes, find its median.
 *  This file contains the definitions of methods used for locating medians
 *  in the distributed data.
 *  See README for details.
 */


#include<Median_Engine.h> 

using namespace std;

bool 
Validation::validateInputs(myssize_t Nodes, myssize_t arraySize) 
{
	// method to reject bad inputs 
	if (!Nodes || Nodes < 0) {

		printf("You have entered %d node\n", Nodes); 
		printf("We need at least 1 to continue; please try again\n");
		return false;

	} else if (Nodes == 1) { 

		printf("You have entered %d Node\n", Nodes); 

	} else { 

		printf("You have entered %d Nodes\n", Nodes); 
	}

	if (!arraySize || arraySize < 0) {

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
		printf("\nallocation failure (nodesArray)\n");
		return; 
	}

	for (mysize_t i = 0; i < Nodes; ++i) 
	{
		// create numNodes arrays 
		nodesArray[i] = new myssize_t[numElements]; 
		if (!nodesArray[i]) { 
			printf("\nallocation failure (nodesArray i %d)\n", i);
			return; 
		}
	}

	numElementsPerArray = numElements;
	infm = new InterNodeFlushManager(this);
	distributedMedian = new DistributedMedian(this, infm);
	rac = new ReadAheadCache(this, infm);

#ifdef STATS_ENABLED
	InterNodeMessages = 0 ;
	cacheHits = 0;
#endif

}

void
DistributedArrayManager::printAllArrays() 
{

	// prints the current state of the distributed arrays

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
	// by default uses quickSelect uses the iterative approach.
	// the recursive method is coded but commented out (don't blow the stack fears!)

	// very basic algorithm is developed to monitor progress of quickSelect. If it isn't
	// found sufficient, then switch over to median of medians for worst-case linear runtime.

	//distributedMedian->quickSelectRecursive(0, totalElements -1, totalElements/2);
	//double qs_median = distributedMedian->checkForMedian(totalElements/2);
	double qs_median = distributedMedian->quickSelect();
	if (distributedMedian->didQSComplete() == true)
		return qs_median;

	// median of medians
	vector<myssize_t> dvector; // since our data is in arrays
	mysize_t median = getTotalElements()/2; 
	return distributedMedian->medianOfMedians(dvector, median, 0, getTotalElements() - 1);
}

double
DistributedArrayManager::validateMedian()
{

	// median validation method (DEBUG ONLY)
	// used to validate the median found via either of the algorithms over
	// the distributed data.

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
	// bypasses the cache and directly writes to the node. 
	// probably used by a batch flush 
	// A batch message is created and all (dirty) elements for that node
	// are sent as one message.  
	myssize_t* nArray = nodesArray[getNode(index)]; 
	mysize_t rindex = index - getNode(index) * numElementsPerArray; 
	nArray[rindex] = element; 
}

void
DistributedArrayManager::setElement(mysize_t index, myssize_t element)
{
	// ONLY sets the cache (normal operation mode)
	// flush manager will clean it eventually (or it may be updated in-place
	// and that saves another update which would have happened otherwise)
	infm->queuePair(index, element, true); // dirty == true
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
#ifdef STATS_ENABLED
		++cacheHits;
#endif
		return element;
	}

	// ReadAheadCache->fetchElements(bool forward, bool backward); (uno internode message)
	// Now the element and its neighbours are in the cache (these elements are in clean state)
	// clean: don't need to be flushed back to the node (can be discarded by LRU: LRU TODO)
	// get it from the cache now
	rac->fetchElements(index, true, true);

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


	myssize_t* nArray = nodesArray[getNode(index)]; 
	mysize_t rindex = index - getNode(index) * numElementsPerArray; 
	return nArray[rindex];
}

InterNodeFlushManager::InterNodeFlushManager(DistributedArrayManager* d) : dam(d) 
{


	// this object runs the flushing of dirty entries in the cache to their respective nodes.
	// A batched message is created to send all the dirty entries in one message for a particular
	// node.

	mysize_t nodes = dam->getNumNodes();
	//BatchNodes = new BatchInfo**[nodes]; // Time permitting; more elaborate per node BatchNode
	BatchNodes = new BatchInfo*[nodes];
	if (!BatchNodes) {
		printf("Unable to allocate BatchedNode\n");
		return; // TODO: pass status as arg
	}

	for (mysize_t i = 0; i < nodes; ++i) { 

		BatchNodes[i] = new BatchInfo(i);
		if (!BatchNodes[i]) {
			printf("Unable to allocate BatchedNode[]\n");
			return; // TODO: pass status as arg
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
#ifdef DEBUG_ENABLED
		printf("swapValues: Not queuing index1 %d, index2 %d - they are same\n", index1, index2); 
#endif
		return;
	}
#ifdef DEBUG_ENABLED
	printf("swapValues: index1 %d, index2 %d\n", index1, index2);
#endif

	mysize_t element1 = dam->getElementAtIndex(index1); // takes advantage of read ahead
	mysize_t element2 = dam->getElementAtIndex(index2);
#ifdef DEBUG_ENABLED
	printf("swapValues: got values element1 %d, element2 %d\n", element1, element2);
#endif

	queuePair(index2, element1, true); // indexes and values swapped
	queuePair(index1, element2, true); // dirty = true

}

bool
InterNodeFlushManager::checkCache(mysize_t index, myssize_t& element)
{

	// this is the read path (getElementXX) -- always check the cache first
	// there might be an updated copy sitting dirty in the cache.

	mysize_t node = dam->getNode(index);
	vector<std::tuple<mysize_t, myssize_t, bool>>::iterator it;
	for (it = BatchNodes[node]->ev.begin(); it != BatchNodes[node]->ev.end(); ++it) {

		if (std::get<0>(*it) == index) { 
			// found the pair in the list
			// there can only be one such pair for that index in the list
#ifdef DEBUG_ENABLED
			printf("queuePair: found check index %d, element %d, node %d, szv %lu\n", get<0>(*it), get<1>(*it), node, BatchNodes[node]->ev.size()); 
#endif
			element = get<1>(*it);
			return true;
		}
	}
	return false;
}

void
InterNodeFlushManager::queuePair(mysize_t index, myssize_t element, bool dirty)
{

	// Note: ReadAheadCache (queuePair to implement LRU eviction)
	// might need to internally call flush()i -- if elements are dirty?


	// queue up the element to swap
	// flush will get to all the elements destined for a node

	mysize_t node = dam->getNode(index);
	bool found = false; 
	vector<std::tuple<mysize_t, myssize_t, bool>>::iterator it;
	for (it = BatchNodes[node]->ev.begin(); it != BatchNodes[node]->ev.end(); ++it) {

		if (get<0>(*it) == index) { 
			// found the pair in the list; just update it (saves memory)
#ifdef DEBUG_ENABLED
			//printf("queuePair: found index %d, element %d, node %d, szv %lu\n", it->first, it->second, node, BatchNodes[node]->ev.size()); 
			printf("queuePair: found index %d, element %d, node %d, szv %lu\n", get<0>(*it), get<1>(*it), node, BatchNodes[node]->ev.size()); 
			printf("queuePair: Updated to index %d, element %d, node %d, szv %lu\n", index, element, node, BatchNodes[node]->ev.size()); 
#endif
			//it->second = element; 
			get<1>(*it) = element; 
			found = true;
			break;
		}
	}

	if (!found)
		BatchNodes[node]->ev.push_back(std::tuple<mysize_t, myssize_t, bool>(index, element, dirty));


#ifdef DEBUG_ENABLED
	printf("queuePair: index %d, element %d, node %d, szv %lu\n", index, element, node, BatchNodes[node]->ev.size());
#endif
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
		vector<std::tuple<mysize_t, myssize_t, bool>>::iterator it;
		it = BatchNodes[n]->ev.begin();
		for(mysize_t i = 0;  i < BatchNodes[n]->ev.size(); ++i, ++it)
		{
			// this encapsulates a message for all data and sends it to node 'n'
			// a worker recieves the message and then swaps in the new values (local)
			// TODO: LRU eviction
#ifdef DEBUG_ENABLED
			printf("flush found index %d, element %d, node %d, szv %lu\n", it->first, it->second, n, BatchNodes[n]->ev.size()); 
#endif
			if (get<2>(*it) == true) {
				dam->setElementFlush(get<0>(*it), get<1>(*it)); // TODO: Add dirty check
				get<2>(*it) = false; // turn off the dirty bit in the cache
			}
		}
#ifdef STATS_ENABLED
        // we intend for all indexes to a node  to be packaged into a message (subject to the transport protocol)
        // and count as one inter node message (batched mode)
		dam->incrInterNodeMessages();
#endif
	}
}

void
DistributedMedian::insertionSortDistributed(mysize_t left, mysize_t right)
{

	// this method is customized to run on the distributed data. 
	// such methods will greatly be helped with the Read Ahead Cache.
	// used during median of medians or to finish out small sized data.

	mysize_t i;
	for(i = right; i > left; i--) compexch(i-1, i);
	for(i = left + 2; i <= right; i++) {
		mysize_t j = i; myssize_t v = dam->getElementAtIndex(i); // low cost
		while (j > 0 && v < dam->getElementAtIndex(j-1))
		{ dam->setElement(j, dam->getElementAtIndex(j-1)); j--; } 
		dam->setElement(j, v);
	}
	infm->flush();
}

bool
DistributedMedian::checkQuickSelectProgress(mysize_t pivotIndex)
{

	// this method is extremely important.
	// determines whether quickselect is making good progress. 
	// a lot more thought can be put into this.
	// as presently constructed, its trying to monitor how far dispersed
	// subsequent pivotIndexes are.

	myssize_t udiff  = pivotIndex - lastPivotIndex; 
	mysize_t  diff = abs(udiff);
	if (diff < diffIndex) {
		++progressCount;
		if (progressCount > cutoff) {
			printf("quick select not making sufficient progress, pc %d\n", progressCount); 
			printf("switching over to median of medians\n"); 
			return true;
		}
	}
#ifdef DEBUG_ENABLED
	printf("diff %d, di %d, lp %d, pi %d\n", diff, diffIndex, lastPivotIndex, pivotIndex);
#endif
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
DistributedMedian::exchFlush(mysize_t index1, mysize_t index2) 
{
	// < only for testing >
	// exchange the values at index1 & index2
	// However, its an sync operation 
	myssize_t element1 = dam->getElementAtIndex(index1);
	myssize_t element2 = dam->getElementAtIndex(index2);
	dam->setElementFlush(index1, element2);
	dam->setElementFlush(index2, element1);
}

void
DistributedMedian::compexch(mysize_t index1, mysize_t index2)
{

	// compare and exchange if condition is met.

	if (dam->getElementAtIndex(index2) < dam->getElementAtIndex(index1))
		exch(index1, index2);

}

mysize_t
DistributedMedian::partition(mysize_t left, mysize_t right, myssize_t pivotElement) 
{
	// classic parition (around the pivot element)
	// picks the right most element randomly to pivot around
	// quickSelect (MEDIAN_THREE) by default is using median of three partioning 
	// to set that last element.
	mysize_t i = left - 1, j = right;
	myssize_t v;

	if (pivotElement) {
		// mom supplies a pivot element
		v = pivotElement;
	} else {
		// randomly chooses to partition with the last element
		v = dam->getElementAtIndex(right);
	}

#ifdef DEBUG_ENABLED
	printf("partition i %d, j %d element %d, pe %d\n", i,j, v, pivotElement); 
#endif
	for(;;)
	{

		while (dam->getElementAtIndex(++i) < v) ; 
		while (j > 0 && v < dam->getElementAtIndex(--j)) if (j == left) break;
		if (i >= j) break;
		else if(dam->getElementAtIndex(i) == dam->getElementAtIndex(j)) {++i; continue;} // duplicate elements
#ifdef DEBUG_ENABLED
		printf("exchange i %d, j %d, elementsi %d, elementj %d\n", i,j,dam->getElementAtIndex(i),dam->getElementAtIndex(j)); 
#endif
		exch(i, j);
	}
	if (!pivotElement)
		exch(i, right);  // don't exchange the last element if we supplied a pivot (MOM)

	infm->flush(); // TODO: can this be further delayed? (upto one message to each node possible here)

#ifdef DEBUG_ENABLED
	printf("returning pivot i %d\n", i);
	dam->printAllArrays();
#endif
	return i; // pivot
}

void
DistributedMedian::quickSelectRecursive(mysize_t l, mysize_t r, mysize_t k) 
{

	// recursive quick select (Unused currently in the code)
	// stack depth is always a point of concern (for me).

	if (r<= l) return;
	mysize_t i = partition(l, r);
	if (i > k) quickSelectRecursive(l, i-1, k); 
	if (i < k) quickSelectRecursive(i+1, r, k);
}

double
DistributedMedian::quickSelect() 
{
	// random selection of pivot (last element or something else)
	// iterative to eliminate tail recursion

	// Median-of-three:
	// median-of-three with a cutoff for small subfiles/arrays
	// can improve the running time of quicksort/select by 20 to 25 percent


	mysize_t left = 0; 
	mysize_t right = dam->getTotalElements() - 1;
	mysize_t median = dam->getTotalElements()/2; // index
#ifdef DEBUG_ENABLED
	printf("enter quick select r %d, m %d, \n", right, median);
#endif

	while (right > left) {
#ifdef MEDIAN_THREE
		exch((left+right)/2, right -1);
		compexch(left, right - 1);
		compexch(left, right);
		compexch(right - 1, right);
#endif
		mysize_t pivotIndex = partition(left, right);
		if (checkQuickSelectProgress(pivotIndex)) // true return implies not good progress (possibly O(n^2))
			return 0; // will be ignored
#ifdef DEBUG_ENABLED
		printf("partition pivotIndex %d\n",pivotIndex); //TODO: DEBUG REMOVE
#endif
		if (pivotIndex >= median) right = pivotIndex - 1; 
		if (pivotIndex <= median) left  = pivotIndex + 1; 
	}

	return checkForMedian(median);
}


double 
DistributedMedian::checkForMedian(mysize_t pivotIndex)
{

	// calculate median subject to number of total elements

	if (dam->getTotalElements() % 2 == 0) { 
		int i = dam->getElementAtIndex(pivotIndex - 1);
		int k = dam->getElementAtIndex(pivotIndex );
		double j = (i + k)/2.;
#ifdef DEBUG_ENABLED
		printf("even elements pi %d, i %d, k %d, median %lf\n", pivotIndex,i, k, j); //TODO: REMOVE
#endif
		return j;
	} 
	return dam->getElementAtIndex(pivotIndex);
}



double
DistributedMedian::medianOfMedians(vector<myssize_t>& vec, mysize_t k, mysize_t start, mysize_t end)
{
	// if the data set is small, sort it and return the median
	if(end - start < 10) {
		insertionSortDistributed(start, end);
		return checkForMedian(k);
	}

	return medianOfMediansFinal(vec, k, start, end);

}

double
DistributedMedian::medianOfMediansFinal(vector<myssize_t>& vec, mysize_t k, mysize_t start, mysize_t end, bool mom)
{

	// see README for a detailed breakdown on median of medians. 
	// In a nutshell, attempt to find a pivot which will likely 
	// split the data in near equal halves (best case); thus guaranteeing
	// worst case linear time. 

	// recursive method (watch out for the recursive depth here)
	// (add bounds check (check stack size on stacking every new
	// frame on the stack) -- TODO: get stack size).

#ifdef DEBUG_ENABLED
	printf("enter MoM l, %d, r %d, k %d\n", start, end, k);
#endif

	// base case
	if(end - start < 10) {
		insertionSortDistributed(start, end);
		if (mom) 
			return vec.at(k);
		else 
			return checkForMedian(k + start);
	}

	vector<myssize_t> medians;
	/* sort every 5 (could also use 7 or 9)*/
	for(mysize_t i = start; i < end; i+=5){
		if(end - i < 10){
			insertionSortDistributed(i, end);
			medians.push_back(dam->getElementAtIndex((i+end)/2));
		}
		else{
			insertionSortDistributed(i, i+5);
			medians.push_back(dam->getElementAtIndex(i+2));
		}
	}

	myssize_t median = medianOfMediansFinal(medians, medians.size()/2, 0, medians.size(), true);

#ifdef DEBUG_ENABLED
	printf("got back median MoM %d\n", median);
#endif
	mysize_t piv = partition(start, end, median);
	mysize_t length = piv - start + 1;

	if(k < length) {
#ifdef DEBUG_ENABLED
		printf("< k after part piv %d, st %d, end %d, l %d,k %d\n", piv, start, end, length, k);
#endif
		return medianOfMediansFinal(vec, k, start, piv);
	}
	else if(k > length){
#ifdef DEBUG_ENABLED
		printf("> k after part piv %d, st %d, end %d, l %d,k %d\n", piv, start, end, length, k);
#endif
		return medianOfMediansFinal(vec, k-length, piv+1, end);
	}
	else {
#ifdef DEBUG_ENABLED
		printf("= k after part piv %d, st %d, l %d,k %d\n", piv, start, length, k);
#endif
		return checkForMedian(k);
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
	mysize_t back_index = (rindex - MEM_THRESH < 0)? 0 : rindex - MEM_THRESH;

#ifdef DEBUG_ENABLED
	printf("fetchElements: fI %d, bi %d, ri %d\n", forward_index, back_index, rindex);
#endif

#ifdef STATS_ENABLED
	dam->incrInterNodeMessages();
#endif
	if (forward) { 

		// probably in partition where both side elements are needed
		// TODO: heuristics
		// how many elements do we want to bring to the cache?
		for (mysize_t i = 0; i < forward_index; ++i) { 
			infm->queuePair(index + i , dam->getElementNoCache(index + i ), false); // dirty = false
		} 

	}

	if (back) { 
		for (mysize_t i = rindex; i < back_index; --i) { 
			infm->queuePair(index - i, dam->getElementNoCache(index - i ), false); // dirty = false
		}
	}
}

int main(int argc, char **argv) 
{

	myssize_t numNodes, numElements;
	printf("please enter number of nodes and hit enter\n");
        char sep;
        if (scanf("%d%c", &numNodes, &sep) != 2 || sep != '\n') {
	    printf("bad input (positive integers expected)\n");
            return -1;
        }
	printf("please enter number of elements of an array (all arrays will be same size), hit enter\n"); 
        if (scanf("%u%c", &numElements, &sep) != 2 || sep != '\n') {
	    printf("bad input (positive integers expected)\n");
            return -1;
        }
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
	printf("There are two options to enter elements: manually (Y below) or autoGenerate (N below) (arrays will be printed to stdout)\n\n");
	printf("Would you like to enter elements of each array (Y/N)?\n");
	scanf(" %c", &ch);

	if (ch == 'y' || ch == 'Y' ) {

		printf("lets enter elements of each array\n");
		for (myssize_t n = 0; n < numNodes; n++) {
			printf("lets enter %u elements of %d array, space separated\n", numElements, n + 1);
			myssize_t element;
			for (myssize_t j = 0; j < numElements; ++j) {
                                char sep;
                                if (scanf("%d%c", &element, &sep) !=2 || sep != ' ') {
	                            printf("bad input (signed integers expected)\n");
                                    return -1;
                                }
				Dam->setElementFlush( (n * numElements) + j, element);
#ifdef VALIDATION_ENABLED
				Dam->medianValidate.push_back(element);
#endif
			}
		}
	} else { 
		printf("autoGenerate will enter elements of each array\n");
		for (myssize_t n = 0; n < numNodes; n++) {
			mysize_t element;
			for (myssize_t j = 0; j < numElements; ++j) {
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



	printf("\nCalculating median.....\n"); 

	double median = Dam->findMedian();

	printf("\n  median found is %lf\n", median);

#ifdef VALIDATION_ENABLED
	double vmedian =  Dam->validateMedian();
	printf("\n  Validated Median found is %f\n", vmedian); 
#endif

#ifdef STATS_ENABLED
	printf("\n STATS\n"); 
	printf("\n -----\n"); 
	printf("\n * InterNode messages passed (ReadAheadCache is enabled)  = %d\n", Dam->getInterNodeMessages()); 
	printf("\n * Cache Hits =  %d\n", Dam->getCacheHits()); 
	printf("\n * Was Median of Medians used = %d\n", Dam->momUsed()); 
	//printf("\n run time %d\n", dam->runTime());  // TODO
#endif

	printf("\n Final Array standings...\n\n"); 
	Dam->printAllArrays();


	return 0;

}

