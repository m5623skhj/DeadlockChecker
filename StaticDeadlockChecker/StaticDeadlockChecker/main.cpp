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
#include <algorithm>
#include <ranges>
#include <optional>

struct LockDependency
{
    LockDependency(const uintptr_t from, const uintptr_t to, std::string leftLockName, std::string rightLockName, std::string leftLock, std::string rightLock)
        : fromLock(from)
        , toLock(to)
        , fromName(std::move(leftLockName))
        , toName(std::move(rightLockName))
        , fromLocation(std::move(leftLock))
        , toLocation(std::move(rightLock))
        , threadId(std::this_thread::get_id())
    {
    }

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
    static GlobalLockOrderTracker& GetInstance()
    {
        static GlobalLockOrderTracker instance;
        return instance;
    }

    bool RecordLockAcquisition(uintptr_t lockId, const std::string& lockName, const std::source_location& loc = std::source_location::current())
    {
        std::lock_guard guard(trackerMutex);
        std::string locationStr = std::string(loc.file_name()) + ":" + std::to_string(loc.line());
        const auto threadId = std::this_thread::get_id();
        auto& currentLocks = currentLocksPerThread[threadId];

        for (uintptr_t existingLock : currentLocks)
        {
            if (auto key = std::make_pair(existingLock, lockId); depSet.insert(key).second)
            {
                dependencies.emplace_back(existingLock, lockId, FindLockName(existingLock), lockName, "previous_call", locationStr);
            }
        }

        currentLocks.push_back(lockId);
        return CheckForCycles();
    }

    void RecordLockRelease(const uintptr_t lockId)
    {
        std::lock_guard guard(trackerMutex);
        auto& locks = currentLocksPerThread[std::this_thread::get_id()];
        std::erase(locks, lockId);

        if (locks.empty())
        {
            currentLocksPerThread.erase(std::this_thread::get_id());
        }
    }

    [[nodiscard]]
    bool HasLock(const uintptr_t lockId) const
    {
        std::lock_guard guard(trackerMutex);
        const auto it = currentLocksPerThread.find(std::this_thread::get_id());
        if (it == currentLocksPerThread.end())
            return false;

        const auto& locks = it->second;
        return std::ranges::find(locks, lockId) != locks.end();
    }

    void PrintStatistics() const
    {
        std::lock_guard guard(trackerMutex);
        std::cout << "Total Dependencies : " << dependencies.size() << "\n";
    }

private:
    [[nodiscard]]
    std::string FindLockName(const uintptr_t lockId) const
    {
        for (const auto& dep : dependencies)
        {
            if (dep.fromLock == lockId) return dep.fromName;
            if (dep.toLock == lockId) return dep.toName;
        }
        return "unknown";
    }

    bool CheckForCycles()
    {
        std::unordered_map<uintptr_t, std::vector<uintptr_t>> graph;
        for (const auto& [from, to] : depSet)
        {
            graph[from].push_back(to);
        }

        std::unordered_set<uintptr_t> visited, recStack;
        std::vector<uintptr_t> path;

        for (const auto& from : graph | std::views::keys)
        {
            if (Dfs(from, graph, visited, recStack, path))
            {
                ReportDeadlock(path);
                return false;
            }
        }

        return true;
    }

    static bool Dfs(const uintptr_t node,
        const std::unordered_map<uintptr_t, std::vector<uintptr_t>>& graph,
        std::unordered_set<uintptr_t>& visited,
        std::unordered_set<uintptr_t>& recStack,
        std::vector<uintptr_t>& path)
    {
        if (!visited.insert(node).second)
        {
            return false;
        }

        recStack.insert(node);
        path.push_back(node);

        if (const auto it = graph.find(node); it != graph.end())
        {
            for (uintptr_t neighbor : it->second)
            {
                if (recStack.contains(neighbor))
                {
                    path.push_back(neighbor);
                    return true;
                }
                if (Dfs(neighbor, graph, visited, recStack, path))
                {
                    return true;
                }
            }
        }

        recStack.erase(node);
        path.pop_back();

        return false;
    }

    void ReportDeadlock(const std::vector<uintptr_t>& cyclePath) const
    {
        std::cout << "\n[DEADLOCK DETECTED]\nCycle detected in lock dependencies.\nCycle path:\n";

        for (size_t i = 0; i + 1 < cyclePath.size(); ++i)
        {
            const uintptr_t fromLock = cyclePath[i];
            const uintptr_t toLock = cyclePath[i + 1];

            const LockDependency* dep = nullptr;
            for (const auto& dependency : dependencies)
            {
                if (dependency.fromLock == fromLock && dependency.toLock == toLock)
                {
                    dep = &dependency;
                    break;
                }
            }

            if (dep)
            {
                std::cout << "  " << dep->fromName << " (" << dep->fromLocation << ") -> "
                    << dep->toName << " (" << dep->toLocation << ")\n";
            }
            else
            {
                std::cout << "  Unknown dependency from " << fromLock << " to " << toLock << "\n";
            }
        }
    }

private:
    mutable std::mutex trackerMutex;
    std::vector<LockDependency> dependencies;
    std::set<std::pair<uintptr_t, uintptr_t>> depSet;
    std::map<std::thread::id, std::vector<uintptr_t>> currentLocksPerThread;
};

class TrackedMutexWrapper
{
public:
    explicit TrackedMutexWrapper(std::string inLockName)
        : lockName(std::move(inLockName))
    {
    }

    void Lock(const std::source_location& loc = std::source_location::current())
    {
        GlobalLockOrderTracker::GetInstance().RecordLockAcquisition(reinterpret_cast<uintptr_t>(this), lockName, loc);
    }

    void Unlock()
    {
        GlobalLockOrderTracker::GetInstance().RecordLockRelease(reinterpret_cast<uintptr_t>(this));
    }

    std::mutex& Inner()
    {
	    return inner;
    }

private:
    std::string lockName;
    std::mutex inner;
};

class TrackedLockGuard
{
public:
    explicit TrackedLockGuard(TrackedMutexWrapper& mutex, const std::source_location& loc = std::source_location::current())
        : target(mutex)
        , acquired(false)
    {
        const auto lockId = reinterpret_cast<uintptr_t>(&target);
        if (const auto& tracker = GlobalLockOrderTracker::GetInstance(); tracker.HasLock(lockId))
        {
            std::cout << "[Warning] Duplicate lock attempt ignored on: " << loc.file_name() << ":" << loc.line() << "\n";
            return;
        }

        target.Lock(loc);
        guard.emplace(target.Inner());
        acquired = true;
    }

    ~TrackedLockGuard()
    {
        if (acquired)
        {
            target.Unlock();
        }
    }

private:
    TrackedMutexWrapper& target;
    std::optional<std::lock_guard<std::mutex>> guard;
    bool acquired;
};

struct TestStruct1
{
    TrackedMutexWrapper mutex{ "TestStruct::mutex_1" };
};

struct TestStruct2
{
    TrackedMutexWrapper mutex{ "TestStruct::mutex_2" };
};

namespace
{
    void Test()
    {
        TestStruct1 t1;
        TestStruct1 t1_1;
        TestStruct2 t2;

        {
            TrackedLockGuard g1(t1.mutex);
            TrackedLockGuard g2(t1_1.mutex);
            TrackedLockGuard g3(t2.mutex);
        }

        {
            TrackedLockGuard g1(t1.mutex);
            TrackedLockGuard g2(t2.mutex);
        }

        {
            TrackedLockGuard g1(t1.mutex);
            TrackedLockGuard g2(t1_1.mutex);
        }

        {
            TrackedLockGuard g1(t1_1.mutex);
            TrackedLockGuard g2(t1_1.mutex);
        }

        GlobalLockOrderTracker::GetInstance().PrintStatistics();
    }
}

int main()
{
    std::cout << "Running static deadlock detection test...\n";
    Test();
    return 0;
}
