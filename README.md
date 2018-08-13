Asssumptions:
------------

* Arrays are unsorted
* Inter-node communication cost is high (i.e. should be avoided)
* duplicates are possible
* no idea about range of numbers
* numbers are signed integers
* almost no extra memory availiable on nodes (other than to hold the arrays)

Goals: 
------

* Find the median
* run time: O(n) (worst case)
* memory : O(1) (other than memory for existing array's) 
* efficient (batch) exchange of information & numbers between nodes
* eliminate tail recursion (if it applies)

	
Our approach:
------------

Highlights: 
---------- 

Before describing the algorithm, we would like to point to a few accomplishments that might be noteworthy(hopefully :))
-------------------------------------------------------------------------------------------------------------------

(Note: In the interest of time, the concepts are mentioned and basic implementation is completed)

1) InterNodeFlushManager: a flush engine which batches elements before sending them to a node -- thereby reducing inter node messaging. 
2) DistributedArrayManager: an array manager that hides the distributed of the data and makes the data look like one big array. 
3) ReadAheadCache: (TODO: might not have time left to finish this): idea is to read elements in batches -- especially for methods like
   sorting or partition which sequentially access elements. Again, saves inter node message exchanges. 

Algorithm: Introselect. 
---------- quickslect, or possibly median-of-medians.

We expect quickSelect to be sufficienty fast enough for most cases; however for cases that exhibit degenerate behaviour, the program will 
attempt to switch over to median-of-medians algorithm to guarantee linear worst case run time. 

Switching to median-of-medians will be determined by sum of the size of all partitions at some point during quickselect. Calculating the pivot with median-of medians comes at a cost, so this switching should to be accurate.

quickselect: 
-- 

quickselect is a selection algorithm to find the kth smallest element in an unordered list. It is related to the quicksort sorting algorithm. Like quicksort, it is efficient in practice and has good average-case performance (O(n)), but has poor worst-case performance. Quickselect and its variants are the selection algorithms most often used in efficient real-world implementations (courtesy: wikipedia) 

quickselect is an in-place algorithm which helps with memory constraints here; also eliminating tail recursion helps. We won't describe quickselect in detail since its readily availiable.

Therefore, if we notice falling into degenerate quickselect behaviour (O(n^2)), then we will switch to median of medians.

TODO: 
quickSelect (median-of -three)
 * median-of-three with a cutoff for small subfiles/arrays
 * can improve the running time of quicksort/select by 20 to 25 percent

median-of-medians:
-- 

the median of medians is an approximate (median) selection algorithm, frequently used to supply a good pivot for an exact selection algorithm, mainly the quickselect, that selects the kth largest element. Median of medians finds an approximate median in linear time only, which is limited but an additional overhead for quickselect. When this approximate median is used as an improved pivot, the worst-case complexity of quickselect reduces significantly from quadratic to linear, which is also the asymptotically optimal worst-case complexity of any selection algorithm (courtesy: wikipedia)

Similarly, Median of medians is used in the hybrid introselect algorithm as a fallback for pivot selection at each iteration until kth largest is found. This again ensures a worst-case linear performance, in addition to average-case linear performance: introselect starts with quickselect (with random pivot, default), to obtain good average performance, and then falls back to modified quickselect with pivot obtained from median of medians if the progress is too slow. Even though asymptotically similar, such a hybrid algorithm will have a lower complexity than a straightforward introselect up to a constant factor (both in average-case and worst-case), at any finite length. (courtesy: wikipedia)

<source wikipedia>
Introselect works by optimistically starting out with quickselect and only switching to a worst-case linear-time selection algorithm (the Blum-Floyd-Pratt-Rivest-Tarjan median of medians algorithm) if it recurses too many times without making sufficient progress. The switching strategy is the main technical content of the algorithm. Simply limiting the recursion to constant depth is not good enough, since this would make the algorithm switch on all sufficiently large lists. Musser discusses a couple of simple approaches:

    :Keep track of the list of sizes of the subpartitions processed so far. If at any point k recursive calls have been made without halving the list size, for some small positive k, switch to the worst-case linear algorithm.
    :Sum the size of all partitions generated so far. If this exceeds the list size times some small positive constant k, switch to the worst-case linear algorithm. This sum is easy to track in a single scalar variable.

Both approaches limit the recursion depth to k ⌈log n⌉ = O(log n) and the total running time to O(n).



Internode communication: 
-----------------------

we assume a spanning tree configurtion for internode communication. We take one node as the coordinator node if necessary(TBD); if so then it should be at the center of the graph. Take advantage of the network topology.

Implementation: 
--------------

Since we have to tackle arrays (of the same size) accross multiple nodes; we have to keep internode communication to a minimum (assume inter-node links are already bandwidth starved).  To achieve this goal, we will attempt to batch data communicatons accross nodes (i.e. saturate the inter-node packet).
class InterNodeFlushManager;

Also there seems a need of an DistributedArrayManager to keep track of all the arrays and manage them, as if to look like one big array. That is, make the diferent nodes and arrays look seamless to the algorithm. Present the size as a summation of all the arrays.
class DistributedArrayManager;

A read ahead cache to pull elements in batches from a node (TODO: implementation)

Important (details): 
  -- cache coherency
  -- data corruption 
  -- synchronization

Statistics: 
  -- With the Read Ahead Cache we can get an idea of how many inter node messages were sent. 
  -- inter node flush manager will also tell us messages passes (write/flush messages) 
  -- Others?



