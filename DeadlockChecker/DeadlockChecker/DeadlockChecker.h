#pragma once
#include <thread>
#include <mutex>
#include <unordered_map>

class DeadlockChecker
{
private:
	DeadlockChecker() = default;

public:
	~DeadlockChecker() = default;
	DeadlockChecker(const DeadlockChecker&) = delete;
	DeadlockChecker& operator=(const DeadlockChecker&) = delete;
	DeadlockChecker(DeadlockChecker&&) = delete;
	DeadlockChecker& operator=(DeadlockChecker&&) = delete;

public:
	static DeadlockChecker& GetInstance()
	{
		static DeadlockChecker instance;
		return instance;
	}

public:
	void Start();
	void Stop();

	[[nodiscard]]
	bool IsRunning() const { return isRunning; }
	// will be applied from the next frame
	void SetDeadlockCheckInterval(const std::chrono::milliseconds intervalMs) { deadlockCheckIntervalMs = intervalMs; }

	void InsertThread(std::thread::id threadId);
	void RemoveThread(std::thread::id threadId);
	void UpdateThreadRespawnTime(const std::thread::id threadId, const std::chrono::milliseconds respawnTimeMs);

private:
	void RunDeadlockThread();

private:
	std::jthread deadlockCheckerThread;
	bool isRunning{};

	// default 60 seconds
	static constexpr std::chrono::milliseconds DEFAULT_DEADLOCK_CHECKER_INTERVAL { 60000 };
	std::chrono::milliseconds deadlockCheckIntervalMs{ DEFAULT_DEADLOCK_CHECKER_INTERVAL };

	static constexpr std::chrono::milliseconds DEADLOCK_CHECK_TIME{ DEFAULT_DEADLOCK_CHECKER_INTERVAL * 2 };

	std::mutex threadBeforeRespawnsTimeMapMutex;
	std::unordered_map<std::thread::id, std::chrono::steady_clock::time_point> threadBeforeRespawnsTimeMap;
};