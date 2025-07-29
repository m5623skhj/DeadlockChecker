#pragma once
#include <thread>

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
	void SetDeadlockCheckInterval(const unsigned long long intervalMs) { deadlockCheckIntervalMs = intervalMs; }

private:
	void RunDeadlockThread();

private:
	std::jthread deadlockCheckerThread;
	bool isRunning{};

	// default 60 seconds
	static constexpr unsigned long long DEFAULT_DEADLOCK_CHECKER_INTERVAL { 60000 };
	unsigned long long deadlockCheckIntervalMs{ DEFAULT_DEADLOCK_CHECKER_INTERVAL };
};