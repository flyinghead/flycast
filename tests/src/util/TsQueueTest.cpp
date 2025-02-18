#include "gtest/gtest.h"
#include "util/tsqueue.h"
#include <atomic>
#include <future>

class TsQueueTest : public ::testing::Test
{
};

TEST_F(TsQueueTest, Basic)
{
	TsQueue<int> queue;
	ASSERT_TRUE(queue.empty());
	ASSERT_EQ(0, queue.size());
	queue.push(42);
	ASSERT_FALSE(queue.empty());
	ASSERT_EQ(1, queue.size());
	ASSERT_EQ(42, queue.pop());

	queue.push(1);
	queue.push(2);
	queue.push(3);
	ASSERT_FALSE(queue.empty());
	ASSERT_EQ(3, queue.size());
	ASSERT_EQ(1, queue.pop());
	ASSERT_EQ(2, queue.pop());
	ASSERT_EQ(3, queue.pop());
}

TEST_F(TsQueueTest, MultiThread)
{
	TsQueue<bool> queue;
	std::atomic<bool> gotResult = false;
	std::future<bool> future = std::async(std::launch::async, [&]() {
		bool res = queue.pop();
		gotResult = true;
		return res;
	});
	usleep(500'000);
	ASSERT_FALSE(gotResult);
	ASSERT_EQ(std::future_status::timeout, future.wait_for(std::chrono::seconds(0)));
	queue.push(true);
	ASSERT_TRUE(future.get());
}

TEST_F(TsQueueTest, Class)
{
	struct T1 {
		float f;
	};
	TsQueue<T1> q1;
	q1.push({ 3.14f });
	T1 r1 = q1.pop();
	ASSERT_EQ(3.14f, r1.f);

	class T2
	{
	public:
		std::string s;
	};
	TsQueue<T2> q2;
	q2.push({ "pi" });
	T2 r2 = q2.pop();
	ASSERT_EQ(std::string("pi"), r2.s);

	// Non copyable, but moveable
	class T3
	{
	public:
		T3(const char *s) : s(s) {}
		T3(const T3&) = delete;
		T3(T3&& other) {
			std::swap(s, other.s);
		}
		T3& operator=(const T3& other) = delete;
		T3& operator=(T3&& other) {
			std::swap(s, other.s);
			return *this;
		}

		const char *s;
	};
	TsQueue<T3> q3;
	q3.push(T3("pi"));
	T3 r3 = q3.pop();
	ASSERT_EQ("pi", r3.s);
}

TEST_F(TsQueueTest, Clear)
{
	TsQueue<const char*> q;
	q.push("a");
	q.push("b");
	q.clear();
	q.push("c");
	ASSERT_EQ(0, strcmp("c", q.pop()));
}
