#include <algorithm> // For std::count
#include <cassert>

#include "movegen.h"
#include "search.h"
#include "thread.h"
#include "uci.h"

ThreadPool Threads; // Global object

/// Thread constructor launches the thread and then waits until it goes to sleep
/// in idle_loop().

Thread::Thread()
{
	resetCalls = exit = false;
	maxPly = callsCnt = 0;
	tbHits = 0;
	history.clear();
	counterMoves.clear();
	idx = Threads.size(); // Start from 0

	std::unique_lock<Mutex> lk(mutex);
	searching = true;
	nativeThread = std::thread(&Thread::idle_loop, this);
	sleepCondition.wait(lk, [&] { return !searching; });
}

/// Thread destructor waits for thread termination before returning

Thread::~Thread() 
{

	mutex.lock();
	exit = true;
	sleepCondition.notify_one();
	mutex.unlock();
	nativeThread.join();
}

/// Thread::wait_for_search_finished() waits on sleep condition
/// until not searching

void Thread::wait_for_search_finished() {

	std::unique_lock<Mutex> lk(mutex);
	sleepCondition.wait(lk, [&] { return !searching; });
}

/// Thread::wait() waits on sleep condition until condition is true

void Thread::wait(std::atomic_bool& condition) {

	std::unique_lock<Mutex> lk(mutex);
	sleepCondition.wait(lk, [&] { return bool(condition); });
}

/// Thread::start_searching() wakes up the thread that will start the search

void Thread::start_searching(bool resume) 
{

	std::unique_lock<Mutex> lk(mutex);

	if (!resume)
		searching = true;

	sleepCondition.notify_one();
}

/// Thread::idle_loop() is where the thread is parked when it has no work to do

void Thread::idle_loop() 
{

	while (!exit)
	{
		std::unique_lock<Mutex> lk(mutex);

		searching = false;

		while (!searching && !exit)
		{
			sleepCondition.notify_one(); // Wake up any waiting thread
			sleepCondition.wait(lk);
		}

		lk.unlock();

		if (!exit)
			search();
	}
}

/// ThreadPool::init() creates and launches requested threads that will go
/// immediately to sleep. We cannot use a constructor because Threads is a
/// static object and we need a fully initialized engine at this point due to
/// allocation of Endgames in the Thread constructor.

void ThreadPool::init() 
{

	push_back(new MainThread);
	read_uci_options();
}

/// ThreadPool::exit() terminates threads before the program exits. Cannot be
/// done in destructor because threads must be terminated before deleting any
/// static objects while still in main().

void ThreadPool::exit() 
{

	while (size())
		delete back(), pop_back();
}

/// ThreadPool::read_uci_options() updates internal threads parameters from the
/// corresponding UCI options and creates/destroys threads to match requested
/// number. Thread objects are dynamically allocated.

void ThreadPool::read_uci_options() 
{

	size_t requested = Options["Threads"];

	assert(requested > 0);

	while (size() < requested)
		push_back(new Thread);

	while (size() > requested)
		delete back(), pop_back();
}

/// ThreadPool::nodes_searched() returns the number of nodes searched

uint64_t ThreadPool::nodes_searched() const 
{

	uint64_t nodes = 0;
	for (Thread* th : *this)
		nodes += th->rootPos.nodes_searched();
	return nodes;
}

/// ThreadPool::tb_hits() returns the number of TB hits

uint64_t ThreadPool::tb_hits() const 
{

	uint64_t hits = 0;
	for (Thread* th : *this)
		hits += th->tbHits;
	return hits;
}

/// ThreadPool::start_thinking() wakes up the main thread sleeping in idle_loop()
/// and starts a new search, then returns immediately.

void ThreadPool::start_thinking(Position& pos, StateListPtr& states,
	const Search::LimitsType& limits)
{
	main()->wait_for_search_finished();

	Search::Signals.stopOnPonderhit = Search::Signals.stop = false;
	Search::Limits = limits;
	Search::RootMoves rootMoves;

	for (const auto& m : MoveList<LEGAL>(pos))
		if (limits.searchmoves.empty()
			|| std::count(limits.searchmoves.begin(), limits.searchmoves.end(), m))
			rootMoves.push_back(Search::RootMove(m));	

	// After ownership transfer 'states' becomes empty, so if we stop the search
	// and call 'go' again without setting a new position states.get() == NULL.
	assert(states.get() || setupStates.get());

	if (states.get())
		setupStates = std::move(states); // Ownership transfer, states is now empty

	StateInfo tmp = setupStates->back();

	for (Thread* th : Threads)
	{
		th->maxPly = 0;
		th->tbHits = 0;
		th->rootDepth = DEPTH_ZERO;
		th->rootMoves = rootMoves;
		th->rootPos.set(pos.fen(), &setupStates->back(), th);
	}

	setupStates->back() = tmp; // Restore st->previous, cleared by Position::set()

	main()->start_searching();
}