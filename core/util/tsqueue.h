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
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class TsQueue
{
public:
	void push(const T& t)
	{
		std::lock_guard<std::mutex> _(mutex);
		queue.emplace(t);
		condVar.notify_one();
	}
	void push(T&& t)
	{
		std::lock_guard<std::mutex> _(mutex);
		queue.push(std::move(t));
		condVar.notify_one();
	}

	T pop()
	{
		std::unique_lock<std::mutex> lock(mutex);
		condVar.wait(lock, [this]() { return !queue.empty(); });
		T t = std::move(queue.front());
		queue.pop();
		return t;
	}

	size_t size() const {
		std::lock_guard<std::mutex> _(mutex);
		return queue.size();
	}
	bool empty() const {
		std::lock_guard<std::mutex> _(mutex);
		return queue.empty();
	}

	void clear()
	{
		std::queue<T> empty;
		std::lock_guard<std::mutex> _(mutex);
		std::swap(queue, empty);
	}
	// TODO bool tryPop(T& t, std::chrono::duration timeout) ?

private:
	std::queue<T> queue;
	mutable std::mutex mutex;
	std::condition_variable condVar;
};
