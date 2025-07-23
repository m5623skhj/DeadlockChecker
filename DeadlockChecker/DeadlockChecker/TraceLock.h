#pragma once
#include <mutex>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <set>
#include <thread>
#include <string>
#include <iostream>
#include <source_location>
#include <optional>

struct LockDependency
{
    LockDependency(uintptr_t from, uintptr_t to, std::string leftLockName, std::string rightLockName, std::string leftLock, std::string rightLock);

    uintptr_t fromLock;
    uintptr_t toLock;
    std::string fromName;
    std::string toName;
    std::string fromLocation;
    std::string toLocation;
    std::thread::id threadId;
};

class GlobalLockOrderTracker
{
public:
    static GlobalLockOrderTracker& GetInstance();

public:
    bool RecordLockAcquisition(uintptr_t lockId, const std::string& lockName, const std::source_location& loc = std::source_location::current());
    void RecordLockRelease(uintptr_t lockId);

    [[nodiscard]]
    bool HasLock(uintptr_t lockId) const;

    void PrintStatistics() const;

private:
    [[nodiscard]]
    std::string FindLockName(uintptr_t lockId) const;
	[[nodiscard]]
    bool CheckForCycles();

    static bool Dfs(uintptr_t node,
        const std::unordered_map<uintptr_t, std::vector<uintptr_t>>& graph,
        std::unordered_set<uintptr_t>& visited,
        std::unordered_set<uintptr_t>& recStack,
        std::vector<uintptr_t>& path);
    void ReportDeadlock(const std::vector<uintptr_t>& cyclePath) const;

private:
    mutable std::mutex trackerMutex;
    std::vector<LockDependency> dependencies;
    std::set<std::pair<uintptr_t, uintptr_t>> depSet;
    std::map<std::thread::id, std::vector<uintptr_t>> currentLocksPerThread;
};

class TrackedMutexWrapper
{
public:
    explicit TrackedMutexWrapper(std::string inLockName);

    void Lock(const std::source_location& loc = std::source_location::current());
	void Unlock();
    std::mutex& Inner();

private:
    std::string lockName;
    std::mutex inner;
};

class TrackedLockGuard
{
public:
    explicit TrackedLockGuard(TrackedMutexWrapper& mutex, const std::source_location& loc = std::source_location::current());
    ~TrackedLockGuard();

private:
    TrackedMutexWrapper& target;
    std::optional<std::lock_guard<std::mutex>> guard;
    bool acquired;
};