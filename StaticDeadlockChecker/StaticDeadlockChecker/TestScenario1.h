#pragma once
#include <random>
#include "TraceLock.h"

struct Player
{
    TrackedMutexWrapper lock{ "Player" };
};

struct Monster
{
    TrackedMutexWrapper lock{ "Monster" };
};

struct DropTable
{
    TrackedMutexWrapper lock{ "DropTable" };
};

inline void TestScenario1()
{
    constexpr int kNumPlayers = 1000;
    constexpr int kNumMonsters = 1000;
    constexpr int kNumThreads = 16;

    std::vector<Player> players(kNumPlayers);
    std::vector<Monster> monsters(kNumMonsters);
    DropTable dropTable;

    std::atomic running = true;
    std::vector<std::thread> threads;

    auto worker = [&](const int threadId)
        {
            std::mt19937 rng(threadId + std::random_device{}());
            std::uniform_int_distribution<> distP(0, kNumPlayers - 1);
            std::uniform_int_distribution<> distM(0, kNumMonsters - 1);
            std::uniform_int_distribution<> distAction(0, 2);

            while (running)
            {
                const int pid = distP(rng);
                const int mid = distM(rng);

                if (const int action = distAction(rng); action == 0)
                {
                    TrackedLockGuard g1(players[pid].lock);
                    TrackedLockGuard g2(monsters[mid].lock);
                }
                else if (action == 1)
                {
                    TrackedLockGuard g1(monsters[mid].lock);
                    TrackedLockGuard g2(dropTable.lock);
                }
                else
                {
                    TrackedLockGuard g1(dropTable.lock);
                    TrackedLockGuard g2(players[pid].lock);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };

    threads.reserve(kNumThreads);
    for (int i = 0; i < kNumThreads; ++i)
    {
        threads.emplace_back(worker, i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    running = false;

    for (auto& t : threads)
    {
        t.join();
    }

    GlobalLockOrderTracker::GetInstance().PrintStatistics();
}
