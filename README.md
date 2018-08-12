Asssumptions:
------------

* Arrays are unsorted
* Inter-node communication cost is high (i.e. should be avoided)
* duplicates are possible
* no idea about range of numbers
* numbers are signed integers
* almost no memory availiable on nodes (other than to hold the arrays)

Goals: 
------

* Find the median 
* run time: O(n)
* memory : O(1) (other than memory for existing array's) 
* efficient (batch) exchange of information & numbers between nodes
* eliminate tail recursion (if it applies)

	
lorenzhs on Mar 11, 2017 [-]

Well if you'd like to select the k-th biggest element from a distributed array it's impractical - if not impossible - to send everything to one machine so another approach is needed. A fairly simple algorithm would select a pivot randomly on a random node and broadcast it to all other nodes. Each node then does the partitioning locally and counts partition sizes. Now you do a sum all-reduction on the sizes to decide which side you need to recurse on. This takes logarithmically many rounds in expectation just like the sequential algorithm, but it could result in very unbalanced work if the data isn't distributed randomly. It also has even more trouble if the pivot selection is bad because of the communication overhead. There are various techniques to overcome these challenges but it's a bit much to go into here.


// Suggestion
Divide the numbers into 100 computers (10 MB each). 
Loop until we have one element in each list     
    Find the meadian in each of them with quickselect which is O(N) and we are processing in parallel. The lists will be partitioned at the end wrt median.
    Send the medians to a central computer and find the median of medians. Then send the median back to each computer. 
    For each computer, if the overall median that we just computed is smaller than its median, continue in the lower part of the list (it is already partitioned), and if larger in the upper part.
When we have one number in each list, send them to the central computer and find and return the median.



Our approach:
------------

Algorithm: Introselect.
           quickslect or median-of-medians

We expect quickSelect to be sufficienty fast enough for most cases; however for cases that exhibit degenerate behaviour, the program will 
attempt to switch over to median-of-medians algorithm to guarantee linear worst case run time. 

Switching to median-of-medians will be determined by sum of the size of all partitions at some point during quickselect. Calculating the pivot with median-of medians comes at a cost, so this switching should to be accurate.

quickselect: 
-- 

quickselect is a selection algorithm to find the kth smallest element in an unordered list. It is related to the quicksort sorting algorithm. Like quicksort, it is efficient in practice and has good average-case performance (O(n)), but has poor worst-case performance. Quickselect and its variants are the selection algorithms most often used in efficient real-world implementations (courtesy: wikipedia) 

quickselect is an in-place algorithm which helps with memory constraints here; also eliminating tail recursion helps. We won't describe quickselect in detail since its readily availiable.

Therefore, if we notice falling into degenerate quickselect behaviour (O(n^2)), then we will switch to median of medians.

 function select(list, left, right, k)
     loop
         if left = right
             return list[left]
         pivotIndex := ...     // select pivotIndex between left and right
         pivotIndex := partition(list, left, right, pivotIndex)
         if k = pivotIndex
             return list[k]
         else if k < pivotIndex
             right := pivotIndex - 1
         else
             left := pivotIndex + 1



median-of-medians:
-- 

 the median of medians is an approximate (median) selection algorithm, frequently used to supply a good pivot for an exact selection algorithm, mainly the quickselect, that selects the kth largest element. Median of medians finds an approximate median in linear time only, which is limited but an additional overhead for quickselect. When this approximate median is used as an improved pivot, the worst-case complexity of quickselect reduces significantly from quadratic to linear, which is also the asymptotically optimal worst-case complexity of any selection algorithm (courtesy: wikipedia)

Similarly, Median of medians is used in the hybrid introselect algorithm as a fallback for pivot selection at each iteration until kth largest is found. This again ensures a worst-case linear performance, in addition to average-case linear performance: introselect starts with quickselect (with random pivot, default), to obtain good average performance, and then falls back to modified quickselect with pivot obtained from median of medians if the progress is too slow. Even though asymptotically similar, such a hybrid algorithm will have a lower complexity than a straightforward introselect up to a constant factor (both in average-case and worst-case), at any finite length. (courtesy: wikipedia)

function select(list, left, right, n)
    loop
        if left = right
             return left
        pivotIndex := pivot(list, left, right)
        pivotIndex := partition(list, left, right, pivotIndex)
        if n = pivotIndex
            return n
        else if n < pivotIndex
            right := pivotIndex - 1
        else
            left := pivotIndex + 1


 function partition(list, left, right, pivotIndex)
     pivotValue := list[pivotIndex]
     swap list[pivotIndex] and list[right]  // Move pivot to end
     storeIndex := left
     for i from left to right-1
         if list[i] < pivotValue
             swap list[storeIndex] and list[i]
             increment storeIndex
     swap list[right] and list[storeIndex]  // Move pivot to its final place
     return storeIndex


  function pivot(list, left, right)
     // for 5 or less elements just get median
     if right - left < 5:
         return partition5(list, left, right)
     // otherwise move the medians of five-element subgroups to the first n/5 positions
     for i from left to right in steps of 5
         // get the median of the i'th five-element subgroup
         subRight := i + 4
         if subRight > right:
             subRight := right

         median5 := partition5(list, i, subRight)
         swap list[median5] and list[left + floor((i - left)/5)]

     // compute the median of the n/5 medians-of-five
     return select(list, left, left + floor((right - left) / 5), (right - left)/10 + 1)



<source wikipedia>
Introselect works by optimistically starting out with quickselect and only switching to a worst-case linear-time selection algorithm (the Blum-Floyd-Pratt-Rivest-Tarjan median of medians algorithm) if it recurses too many times without making sufficient progress. The switching strategy is the main technical content of the algorithm. Simply limiting the recursion to constant depth is not good enough, since this would make the algorithm switch on all sufficiently large lists. Musser discusses a couple of simple approaches:

    Keep track of the list of sizes of the subpartitions processed so far. If at any point k recursive calls have been made without halving the list size, for some small positive k, switch to the worst-case linear algorithm.
    Sum the size of all partitions generated so far. If this exceeds the list size times some small positive constant k, switch to the worst-case linear algorithm. This sum is easy to track in a single scalar variable.

Both approaches limit the recursion depth to k ⌈log n⌉ = O(log n) and the total running time to O(n).



Internode communication: 
-----------------------

we assume a spanning tree configurtion for internode communication. We take one node as the coordinator node if necessary(TBD); if so then it should be at the center of the graph.  Take advantage of the network topology.

Implementation: 
--------------

Since we have to tackle arrays (of the same size) accross multiple nodes; we have to keep internode communication to a minimum (assume inter-node links are already bandwidth starved).

To achieve the goal above, we will attempt to batch data communicatons accross nodes (i.e. saturation the inter-node packet).
class InterNodeBatchManager;

Also there seems a need of an DistributedArrayManager to keep track of all the arrays and manage them, as if to look like one big array. That is, make the diferent nodes and arrays look seamless to the algorithm. Present the size as a summation of all the arrays.
class DistributedArrayManager;



