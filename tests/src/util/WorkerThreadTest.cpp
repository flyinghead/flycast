#include "gtest/gtest.h"
#include "util/worker_thread.h"
#include <atomic>
#include <future>

class WorkerThreadTest : public ::testing::Test
{
};

TEST_F(WorkerThreadTest, Basic)
{
	WorkerThread worker{"Test"};
	std::atomic<bool> done = false;
	const auto& task = [&]() {
		done = true;
	};
	worker.run(task);
	usleep(100'000);
	ASSERT_TRUE(done);

	// test restart
	worker.stop();
	done = false;
	worker.run(task);
	usleep(100'000);
	ASSERT_TRUE(done);
}

TEST_F(WorkerThreadTest, MultiThread)
{
	WorkerThread worker{"Test"};
	std::atomic<int> counter = 0;
	const auto& task = [&]() {
		++counter;
	};
	const auto& consumer = [&]() {
		for (int i = 0; i < 100; i++)
			worker.run(task);
	};
	std::future<void> futures[4];
	for (auto& f : futures)
		f = std::async(std::launch::async, consumer);
	for (auto& f : futures)
		f.get();
	worker.stop(); // force all tasks to be executed before stopping
	ASSERT_EQ(std::size(futures) * 100, counter);
}

// There's no guarantee that tasks submitted while the worker is being stopped will
// be executed. But it shouldn't crash.
TEST_F(WorkerThreadTest, StartStop)
{
	WorkerThread worker{"Test"};
	std::atomic<int> counter = 0;
	const auto& task = [&]() {
		++counter;
	};
	const auto& consumer = [&]() {
		for (int i = 0; i < 100; i++)
			worker.run(task);
	};
	std::future<void> future = std::async(std::launch::async, consumer);
	std::future<void> future2 = std::async(std::launch::async, [&]() {
		for (int i = 0; i < 100; i++)
			worker.stop();
	});
	future.get();
	future2.get();
	worker.stop();
	//ASSERT_EQ(100, counter);
}

TEST_F(WorkerThreadTest, Future)
{
	WorkerThread worker{"Test"};
	const auto& task = [](u32 v) -> u32 {
		return v;
	};
	std::future<u32> f = worker.runFuture(task, 42);
	ASSERT_EQ(42, f.get());
}
