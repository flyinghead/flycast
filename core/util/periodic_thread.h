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
#include "stdclass.h"
#include "oslib/oslib.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <functional>
#include "log/Log.h"

class VPeriodicThread
{
public:
	virtual ~VPeriodicThread() {
		stop();
	}

	void start()
	{
		LockGuard _(mutex);
		if (thread.joinable())
			return;
		running = true;
		event.Reset();
		thread = std::thread([this]() {
			ThreadName _(name);
			try {
				init();
				while (true)
				{
					if (period != 0)
						event.Wait(period);
					else
						event.Wait();
					if (!running)
						break;
					doWork();
				}
				term();
			} catch (const std::runtime_error& e) {
				ERROR_LOG(COMMON, "PeriodicThread %s: runtime error %s", name, e.what());
			} catch (...) {
				ERROR_LOG(COMMON, "PeriodicThread %s: uncaught unknown exception", name);
			}
		});
	}

	void stop()
	{
		LockGuard _(mutex);
		running = false;
		event.Set();
		if (thread.joinable())
			thread.join();
	}

	void setPeriod(int period) {
		this->period = period;
	}

	void notify() {
		event.Set();
	}

protected:
	VPeriodicThread(const char *name, int periodMS = 0)
		: name(name), period(periodMS)
	{ }
	virtual void doWork() = 0;
	virtual void init() {}
	virtual void term() {}

private:
	using LockGuard = std::lock_guard<std::mutex>;
	const char *name;
	int period;
	cResetEvent event;
	std::thread thread;
	std::atomic<bool> running = false;
	std::mutex mutex;
};

class PeriodicThread : public VPeriodicThread
{
public:
	template<class F, class... Args>
	PeriodicThread(const char *name, F&& f, Args&&... args)
		: VPeriodicThread(name, 0)
	{
		work = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
	}

private:
	void doWork() override {
		work();
	}

	std::function<void()> work;
};
