Instructions to build and run: 
----------------------------

(download all the files (git clone <path> or untar the tarball)

make  (to build)

./Media_Engine (to run)
(program will ask to input number of nodes followed by elements per array)
(then select whether you'd like to enter the input manually or let the program
(auto generate (using rand()) the elements of the array).
(After a few leftover messages, you should see what the program calculated as median)

Validation:
----------
(if validation is enabled (it is by default -- should be disabled for perf tests), then
(a validated median is also displayed -- (fingers crossed :), they should match).

Sample output: 
--------

<<BEGIN SAMPLE OUTPUT>>

pranit@pranit-ThinkPad-S1-Yoga:~/median-project$ ./Median_Engine 
please enter number of nodes
11
please enter number of elements or size (all arrays will be same size)
15
You have entered 11 Nodes
You have entered each array with 15 number of elements

There are two options to enter elements: manually (Y below) or autoGenerate (N below) (arrays will be printed to stdout)

Would you like to enter elements of each array (Y/N)?
n
autoGenerate will enter elements of each array

elements of 1 array
62  97  -40  -94  -64  -74  -3  -85  64  0  93  2  65  42  26  
elements of 2 array
-3  23  89  51  55  10  87  66  -56  21  45  81  -82  94  -74  
elements of 3 array
32  85  17  -79  16  78  20  7  18  -35  32  -8  92  -22  59  
elements of 4 array
-36  -83  -37  -18  45  60  -31  -22  -93  -93  -24  -19  13  -29  -96  
elements of 5 array
16  80  -17  10  43  -72  17  40  60  -84  -49  38  -67  -80  -7  
elements of 6 array
86  9  35  74  85  -74  63  -69  77  47  -37  -18  -78  -47  -70  
elements of 7 array
99  63  -61  7  -98  -41  77  -87  -55  -82  54  -79  33  -19  66  
elements of 8 array
68  92  -27  32  95  -65  -13  -44  -9  -7  28  -52  -48  75  -22  
elements of 9 array
-93  20  66  36  4  -38  -76  10  0  94  -26  -17  -8  32  89  
elements of 10 array
52  29  -38  50  55  -97  62  67  32  78  54  -59  3  -17  10  
elements of 11 array
23  80  -41  18  45  -8  -91  -2  -4  -14  -14  43  11  3  52  


Calculating median.....

  median found is 9.000000

  Validated Median found is 9.000000

 STATS

 -----

 * InterNode messages passed (ReadAheadCache is enabled) 88 

 * Cache Hits  1019

 Was Median of Medians used = 0

 Final Array standings...


elements of 1 array
-72  -69  -40  -94  -64  -74  -76  -85  -38  -93  -22  -48  -52  -44  -65  
elements of 2 array
-27  -38  -19  -79  -82  -55  -87  -97  -56  -41  -98  -91  -82  -61  -74  
elements of 3 array
-70  -47  -78  -79  -18  -37  -59  -74  -17  -35  -80  -67  -41  -22  -49  
elements of 4 array
-36  -83  -37  -18  -84  -26  -31  -22  -93  -93  -24  -19  -17  -29  -96  
elements of 5 array
-17  -2  3  -14  3  -8  -8  0  -3  -3  -13  2  -8  -9  -7  
elements of 6 array
-7  -4  0  -14  4  7  7  9  10  18  18  16  17  10  20  
elements of 7 array
11  10  17  16  23  21  10  13  20  23  54  51  33  52  55  
elements of 8 array
50  45  62  32  29  26  32  42  32  45  28  47  54  52  43  
elements of 9 array
35  32  32  36  38  59  45  60  40  60  43  55  62  66  89  
elements of 10 array
75  95  92  68  66  77  63  67  99  78  77  63  85  74  86  
elements of 11 array
80  80  92  78  85  94  81  66  87  89  65  93  64  97  94  

pranit@pranit-ThinkPad-S1-Yoga:~/median-project$ 

<<END SAMPLE OUTPUT>>



Assumptions:
------------

* Arrays are unsorted
* Inter-node communication cost is high (i.e. should be avoided)
* duplicates are possible
* no idea about range of numbers
* numbers are signed integers (easily switchable to floats)
* almost no extra memory availiable on nodes (other than to hold the arrays)
* we intend for all indexes to a node to be packaged into a message (subject to the transport protocol)
  and count as one inter node message (batched mode)


Goals: 
------

* Find the median
* run time: O(n) (worst case)
* memory : O(1) (other than memory for existing array's) 
* efficient (batch) exchange of information & numbers between nodes
* efficient algorithms: linear run time (worst case). Also improvements such as median-of-three partioning and eliminate tail recursion etc.

TODO: 
----

These two are very important parts of this project; however due to extentuating circumstances they are not complete yet.
ReadAheadCache (or ReadBehind) will demonstrate how internode communication can be minimized. The cache must distinguish 
between clean and dirty cache entries during time of flush.

* ReadAheadCache (60% complete)
* statistics generator (10% complete)

	
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

we will use the median of three partitioning method. Choosing the median of the first, middle amd final element as the partioning element and cutting off the recursion can improve the performance. This method is a simple example of a probablistic algorithm -- one that uses randomness to achieve good performance wit high probability.

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

A read ahead cache to pull elements in batches from a node (TODO: LRU eviction)

    std::vector<std::tuple<mysize_t, myssize_t, bool>> ev; // bool => dirty bit

This tuple contains an element <element, index, bool> in the cache. The dirty bit indicates to the flush manager
whether it needs to be written out.


Important (details): 
  -- cache coherency
  -- data corruption 
  -- synchronization

Statistics: 
  -- With the Read Ahead Cache we can get an idea of how many inter node messages were sent. 
  -- inter node flush manager will also tell us messages passed (write/flush messages) 
     -- TODO: flush to recognize it's own node and not increment inter node messages passed
  -- cache hits (when internode messaging was avoided due to the read ahead/behind)



