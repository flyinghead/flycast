#include "gtest/gtest.h"
#include "util/periodic_thread.h"
#include <atomic>
#include <chrono>
#include <thread>

class PeriodicThreadTest : public ::testing::Test
{
};

TEST_F(PeriodicThreadTest, Basic)
{
	std::atomic<int> counter = 0;
	PeriodicThread thread = PeriodicThread("Test", [&]() {
		counter++;
	});
	thread.setPeriod(10);
	thread.start();
	std::this_thread::sleep_for(std::chrono::milliseconds(15));
	ASSERT_LT(0, counter);
	int copy = counter;
	std::this_thread::sleep_for(std::chrono::milliseconds(15));
	ASSERT_LT(copy, counter);
	thread.stop();
	copy = counter;
	std::this_thread::sleep_for(std::chrono::milliseconds(15));
	ASSERT_EQ(copy, counter);
}
