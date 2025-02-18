/*
	Copyright 2025 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "tsqueue.h"
#include "oslib/oslib.h"
#include <variant>
#include <thread>
#include <memory>
#include <functional>
#include <future>

class WorkerThread
{
public:
	using Function = std::function<void()>;

	WorkerThread(const char *name) : name(name) {
	}
	~WorkerThread() {
		stop();
	}

	void stop()
	{
		std::lock_guard<std::mutex> _(mutex);
		if (thread != nullptr && thread->joinable())
		{
			queue.push(Exit());
			thread->join();
			thread.reset();
		}
	}

	void run(Function&& task) {
		start();
		queue.push(std::move(task));
	}

	template<class F, class... Args>
	auto runFuture(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>
	{
		using return_type = typename std::result_of<F(Args...)>::type;
		auto task = std::make_shared<std::packaged_task<return_type()>>(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...));

		run([task]() {
			(*task)();
		});
		return task->get_future();
	}

private:
	void start()
	{
		std::lock_guard<std::mutex> _(mutex);
		if (thread != nullptr && thread->joinable())
			return;
		queue.clear();
		thread = std::make_unique<std::thread>([this]()
		{
			ThreadName _(name);
			while (true)
			{
				Task t = queue.pop();
				if (std::get_if<Exit>(&t) != nullptr)
					break;
				Function& func = std::get<Function>(t);
				func();
			}
		});
	}

	const char * const name;
	using Exit = std::monostate;
	using Task = std::variant<Exit, Function>;
	TsQueue<Task> queue;
	std::unique_ptr<std::thread> thread;
	std::mutex mutex;
};
