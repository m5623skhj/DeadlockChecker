#include "TraceLock.h"
#include "TestScenario1.h"

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
	std::cout << "----- Test Finished -----\n\n";

	std::cout << "Running test scenario 1...\n";
	TestScenario1();
	std::cout << "----- Test scenario 1 completed -----\n\n";

    return 0;
}
