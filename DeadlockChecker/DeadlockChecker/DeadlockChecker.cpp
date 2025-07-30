#include "DeadlockChecker.h"

#include <iostream>

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

		{
			std::scoped_lock lock(threadBeforeRespawnsTimeMapMutex);
			threadBeforeRespawnsTimeMap.clear();
		}

		deadlockCheckIntervalMs = DEFAULT_DEADLOCK_CHECKER_INTERVAL;
	}
}

void DeadlockChecker::InsertThread(const std::thread::id threadId)
{
	std::scoped_lock lock(threadBeforeRespawnsTimeMapMutex);
	threadBeforeRespawnsTimeMap.insert({ threadId,  std::chrono::steady_clock::now() });
}

void DeadlockChecker::RemoveThread(const std::thread::id threadId)
{
	std::scoped_lock lock(threadBeforeRespawnsTimeMapMutex);
	threadBeforeRespawnsTimeMap.erase(threadId);
}

void DeadlockChecker::UpdateThreadRespawnTime(const std::thread::id threadId, const std::chrono::milliseconds respawnTimeMs)
{
	std::scoped_lock lock(threadBeforeRespawnsTimeMapMutex);
	threadBeforeRespawnsTimeMap[threadId] = std::chrono::steady_clock::now() + respawnTimeMs;
}

void DeadlockChecker::RunDeadlockThread()
{
	std::list<std::pair<std::thread::id, std::chrono::steady_clock::time_point>> deadlockCheckerList;
	const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	while (isRunning)
	{
		deadlockCheckerList.clear();
		{
			std::scoped_lock lock(threadBeforeRespawnsTimeMapMutex);
			for (const auto& [threadId, respawnTime] : threadBeforeRespawnsTimeMap)
			{
				deadlockCheckerList.emplace_back(threadId, respawnTime);
			}
		}

		for (const auto& [threadId, respawnTime] : deadlockCheckerList)
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - respawnTime) > DEADLOCK_CHECK_TIME)
			{
				std::cout << "Deadlock detected for thread: " << threadId << "\n";
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(deadlockCheckIntervalMs));
	}
}
