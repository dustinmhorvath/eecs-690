// One implementation of a barrier (when used in a loop like that of our
// train simulation, this implemenration requires that "barrier" be
// invoked twice with a different actual parameter for "bCounter"):

/*
void barrier(std::atomic<int>* bCounter, int numExpectedAtBarrier)
{
	bCounter[1]++; // used to know when to reset [0]
	bCounter[0]++; // actual barrier test
	while (bCounter[0] != numExpectedAtBarrier)
		; // busy wait

	// don't reset the counter until all threads have dropped out of
	// the while loop above.
	--bCounter[1];
	while (bCounter[1] > 0)
		;
	bCounter[0] = 0;
}

// Much better implementation of a barrier (does NOT require two
// calls per loop):
void barrier(int& bCounter, std::mutex& bMutex,
	std::condition_variable& bCV, int numExpectedAtBarrier)
{
	std::unique_lock<std::mutex> ulbm(bMutex);

	bCounter++;
	if (bCounter != numExpectedAtBarrier)
		bCV.wait(ulbm);
	else
	{
		bCounter = 0;
		bCV.notify_all();
	}
}

*/

// A final "best" implementation based on creating a class whose sole purpose
// is to provide a barrier.  To use this class, place the class definition
// below into "Barrier.h", do a "#include "Barrier.h" in your program, then
// declare:
//		Barrier aBarrier; // at some sort of global scope
// Then when you are in a function or method where you require the barrier:
//		aBarrier.barrier(numExpected);

// Barrier.h - A class that implements a Barrier

#ifndef BARRIER_H
#define BARRIER_H

#include <condition_variable>
#include <mutex>

/* Usage:
	1. Create an instance of a Barrier class (called, say, "b") that
	   is accessible to, but outside the scope of any thread code that
	   needs to use it.
	2. In the thread code where barrier synchronization is to occur,
	   each thread in the "barrier group" must execute:

	   b.barrier(num); // where "num" is the number of threads in
	                   // the "barrier group"
*/

class Barrier
{
public:
	Barrier() : barrierCounter(0) {}
	virtual ~Barrier() {}

	void barrier(int numExpectedAtBarrier)
	{
		std::unique_lock<std::mutex> ulbm(barrierMutex);

		barrierCounter++;
		if (barrierCounter != numExpectedAtBarrier)
			barrierCV.wait(ulbm);
		else
		{
			barrierCounter = 0;
			barrierCV.notify_all();
		}
	}
private:
	int barrierCounter;
	std::mutex barrierMutex;
	std::condition_variable barrierCV;
};

#endif
