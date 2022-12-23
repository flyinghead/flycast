#pragma once

#if FC_PROFILER

#include "types.h"
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>

#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#define FC_PROFILE_SCOPE_RESERVE_SIZE 128
#define FC_PROFILE_HISTORY_MAX_SIZE 512

namespace fc_profiler
{
	struct ProfileSection
	{
		ProfileSection()
			: function(nullptr)
			, file(nullptr)
			, line(0)
			, scope(0)
		{

		}

		ProfileSection(const char* _function, const char* _file, u32 _line, u32 _scope)
			: function(_function)
			, file(_file)
			, line(_line)
			, scope(_scope)
		{
		}

		const char* function;
		const char* file;
		u32 line;
		u32 scope;
		std::chrono::steady_clock::time_point start;
		std::chrono::steady_clock::time_point end;
	};

	struct ProfileThread
	{
		ProfileThread()
		{
			startTicks = std::chrono::high_resolution_clock::now();
			level = 0;
			historyIdx = 0;
			cachedTime = 0.0;
			scopes.reserve(FC_PROFILE_SCOPE_RESERVE_SIZE);
			memset(history, 0, sizeof(history));
		}

		std::vector<ProfileSection> scopes;
		std::chrono::steady_clock::time_point startTicks;
		std::chrono::steady_clock::time_point endTicks;
		double history[FC_PROFILE_HISTORY_MAX_SIZE];
		u32 level;
		u32 historyIdx;
		std::thread::id threadId;
		std::string threadName;

		struct ResultNode
		{
			ResultNode() : parent(nullptr) {}
			ProfileSection section;
			ResultNode* parent;
			std::vector<ResultNode> children;
		};

		double cachedTime;
		std::vector<ResultNode> cachedResultTree;
		static std::vector<ProfileThread*> s_allThreads;
		static std::recursive_mutex s_allThreadsLock;
	};

	struct ProfileScope
	{
		ProfileScope(const char* function, const char* file, int line)
			: sectionIdx(0)
		{
			if (s_thread)
			{
				ProfileSection section(function, file, line, s_thread->level++);
				section.start = std::chrono::high_resolution_clock::now();
				sectionIdx = s_thread->scopes.size();
				s_thread->scopes.push_back(section);
			}
		}

		~ProfileScope()
		{
			if (s_thread)
			{
				s_thread->scopes[sectionIdx].end = std::chrono::high_resolution_clock::now();
				s_thread->level--;
			}
		}

		size_t sectionIdx;
		static thread_local ProfileThread* s_thread;
	};

	void startThread(const std::string& threadName);
	void endThread(double warningTime = 0.0);
	void drawGUI(const std::vector<ProfileThread::ResultNode>& results);
	void drawGraph(const ProfileThread& profileThread);
	void outputTTY(const std::vector<ProfileThread::ResultNode>& results);
}

#define FC_PROFILE_SCOPE \
	fc_profiler::ProfileScope __profile__scope(__PRETTY_FUNCTION__, __FILE__, __LINE__);

#define FC_PROFILE_SCOPE_NAMED(name) \
	fc_profiler::ProfileScope __profile__scope(name, __FILE__, __LINE__);

#else

namespace fc_profiler
{
	static void startThread(const std::string& threadName) {}
	static void endThread(float warningTime = 0.0) {}
}

#define FC_PROFILE_SCOPE
#define FC_PROFILE_SCOPE_NAMED(name)

#endif
