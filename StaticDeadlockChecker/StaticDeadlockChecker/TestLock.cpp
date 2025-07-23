#include "TraceLock.h"
#include <ranges>

LockDependency::LockDependency(const uintptr_t from, const uintptr_t to, std::string leftLockName, std::string rightLockName, std::string leftLock, std::string rightLock)
	: fromLock(from)
	, toLock(to)
	, fromName(std::move(leftLockName))
	, toName(std::move(rightLockName))
	, fromLocation(std::move(leftLock))
	, toLocation(std::move(rightLock))
	, threadId(std::this_thread::get_id())
{
}

GlobalLockOrderTracker& GlobalLockOrderTracker::GetInstance()
{
	static GlobalLockOrderTracker instance;
	return instance;
}

bool GlobalLockOrderTracker::RecordLockAcquisition(uintptr_t lockId, const std::string& lockName, const std::source_location& loc)
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

void GlobalLockOrderTracker::RecordLockRelease(const uintptr_t lockId)
{
	std::lock_guard guard(trackerMutex);
	auto& locks = currentLocksPerThread[std::this_thread::get_id()];
	std::erase(locks, lockId);
	if (locks.empty())
	{
		currentLocksPerThread.erase(std::this_thread::get_id());
	}
}

bool GlobalLockOrderTracker::HasLock(const uintptr_t lockId) const
{
	std::lock_guard guard(trackerMutex);
	const auto it = currentLocksPerThread.find(std::this_thread::get_id());
	if (it == currentLocksPerThread.end())
	{
		return false;
	}

	const auto& locks = it->second;
	return std::ranges::find(locks, lockId) != locks.end();
}

void GlobalLockOrderTracker::PrintStatistics() const
{
	std::lock_guard guard(trackerMutex);
	std::cout << "Total Dependencies : " << dependencies.size() << "\n";
}

std::string GlobalLockOrderTracker::FindLockName(const uintptr_t lockId) const
{
	for (const auto& dep : dependencies)
	{
		if (dep.fromLock == lockId) return dep.fromName;
		if (dep.toLock == lockId) return dep.toName;
	}

	return "unknown";
}

bool GlobalLockOrderTracker::CheckForCycles()
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

bool GlobalLockOrderTracker::Dfs(const uintptr_t node,
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

void GlobalLockOrderTracker::ReportDeadlock(const std::vector<uintptr_t>& cyclePath) const
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

TrackedMutexWrapper::TrackedMutexWrapper(std::string inLockName)
	: lockName(std::move(inLockName))
{
}

void TrackedMutexWrapper::Lock(const std::source_location& loc)
{
	GlobalLockOrderTracker::GetInstance().RecordLockAcquisition(reinterpret_cast<uintptr_t>(this), lockName, loc);
}

void TrackedMutexWrapper::Unlock()
{
	GlobalLockOrderTracker::GetInstance().RecordLockRelease(reinterpret_cast<uintptr_t>(this));
}

std::mutex& TrackedMutexWrapper::Inner()
{
	return inner;
}

TrackedLockGuard::TrackedLockGuard(TrackedMutexWrapper& mutex, const std::source_location& loc)
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

TrackedLockGuard::~TrackedLockGuard()
{
	if (acquired)
	{
		target.Unlock();
	}
}