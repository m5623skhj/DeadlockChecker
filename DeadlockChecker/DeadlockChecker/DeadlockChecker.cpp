#include "DeadlockChecker.h"

void DeadlockChecker::Start()
{
	if (!isRunning)
	{
		isRunning = true;
		deadlockCheckerThread = std::jthread(&DeadlockChecker::RunDeadlockThread, this);
	}
}

void DeadlockChecker::Stop()
{
	if (isRunning)
	{
		isRunning = false;
		deadlockCheckerThread.request_stop();
		deadlockCheckerThread.join();

		deadlockCheckIntervalMs = DEFAULT_DEADLOCK_CHECKER_INTERVAL;
	}
}

void DeadlockChecker::RunDeadlockThread()
{
	while (isRunning)
	{

		std::this_thread::sleep_for(std::chrono::milliseconds(deadlockCheckIntervalMs));
	}
}