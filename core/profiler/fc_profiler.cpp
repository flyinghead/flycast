#include "fc_profiler.h"
#include "log/LogManager.h"
#include "cfg/option.h"
#include "imgui/imgui.h"
#include "implot/implot.h"
#include <cassert>

namespace fc_profiler
{
	thread_local ProfileThread* ProfileScope::s_thread = nullptr;
	std::vector<ProfileThread*> ProfileThread::s_allThreads;
	std::recursive_mutex ProfileThread::s_allThreadsLock;

	void startThread(const std::string& threadName)
	{
		if (config::ProfilerEnabled)
		{
			if (!ProfileScope::s_thread)
			{
				std::unique_lock<std::recursive_mutex> lock(ProfileThread::s_allThreadsLock);
				ProfileScope::s_thread = new ProfileThread();
				ProfileThread::s_allThreads.push_back(ProfileScope::s_thread);
			}

			ProfileThread& profileThread = *ProfileScope::s_thread;
			profileThread.scopes.clear();
			profileThread.scopes.reserve(FC_PROFILE_SCOPE_RESERVE_SIZE);
			profileThread.level = 0;
			profileThread.startTicks = std::chrono::high_resolution_clock::now();
			profileThread.threadName = threadName;
		}
	}

	void endThread(double warningTime)
	{
		if (config::ProfilerEnabled)
		{
			std::unique_lock<std::recursive_mutex> lock(ProfileThread::s_allThreadsLock);

			if (!ProfileScope::s_thread)
				return;
			ProfileThread& profileThread = *ProfileScope::s_thread;

			std::chrono::high_resolution_clock::time_point endTicks = std::chrono::high_resolution_clock::now();
			std::chrono::microseconds durationMicro = std::chrono::duration_cast<std::chrono::microseconds>(endTicks - profileThread.startTicks);
			profileThread.cachedTime = (double)durationMicro.count() / 1000000;

			if (profileThread.scopes.size() > 0)
			{
				profileThread.cachedResultTree.clear();
				profileThread.cachedResultTree.resize(1);
				ProfileThread::ResultNode* node = &profileThread.cachedResultTree.back();
				ProfileThread::ResultNode* parent = node;

				u32 currScope = 0;

				for (const ProfileSection& section : profileThread.scopes)
				{
					if (section.scope == currScope)
					{
						parent->children.push_back(ProfileThread::ResultNode());
						node = &parent->children.back();
						node->parent = parent;
					}
					else if (section.scope > currScope)
					{
						assert(node);
						node->children.push_back(ProfileThread::ResultNode());
						parent = node;
						node = &node->children.back();
						node->parent = parent;
					}
					else if (section.scope < currScope)
					{
						assert(parent);
						parent->children.push_back(ProfileThread::ResultNode());
						node = &parent->children.back();
						parent = node->parent;
						if (!parent)
							parent = node;
					}

					currScope = section.scope;
					node->section = section;
				}
			}

			if (config::ProfilerOutputTTY && warningTime > 0.0f && profileThread.cachedTime > warningTime)
			{
				WARN_LOG(PROFILER, "=== Profiler =======================================================================================");
				WARN_LOG(PROFILER, "Frame profile on thread \'%s\' exceeded warning time (duration = %.4fs, limit = %.4fs)", profileThread.threadName.c_str(), profileThread.cachedTime, warningTime);
				WARN_LOG(PROFILER, "====================================================================================================");

				outputTTY(profileThread.cachedResultTree);

				WARN_LOG(PROFILER, "====================================================================================================\n");
			}

			profileThread.history[profileThread.historyIdx] = profileThread.cachedTime;
			profileThread.historyIdx = (profileThread.historyIdx + 1) % FC_PROFILE_HISTORY_MAX_SIZE;
		}
	}

	void drawGUI(const std::vector<ProfileThread::ResultNode>& results)
	{
		std::unique_lock<std::recursive_mutex> lock(ProfileThread::s_allThreadsLock);

		for (const ProfileThread::ResultNode& node : results)
		{
			if (node.section.function)
			{
				std::chrono::microseconds scopeTimeMicro = std::chrono::duration_cast<std::chrono::microseconds>(node.section.end - node.section.start);
				double scopeTimeS = (double)scopeTimeMicro.count() / 1000000;
				char text[256];
				std::snprintf(text, 256, "%.3f : %s (%s, %i)", (float)scopeTimeS, node.section.function, node.section.file, node.section.line);
				ImGui::TreeNode(text);
			}

			ImGui::Indent();
			drawGUI(node.children);
			ImGui::Unindent();
		}
	}

	void outputTTY(const std::vector<ProfileThread::ResultNode>& results)
	{
		std::unique_lock<std::recursive_mutex> lock(ProfileThread::s_allThreadsLock);

		for (const ProfileThread::ResultNode& node : results)
		{
			std::chrono::microseconds scopeTimeMicro = std::chrono::duration_cast<std::chrono::microseconds>(node.section.end - node.section.start);
			double scopeTimeS = (double)scopeTimeMicro.count() / 1000000;
			WARN_LOG(PROFILER, "%.4f %*s%s (%s, %i)", scopeTimeS, node.section.scope, "", node.section.function, node.section.file, node.section.line);
			outputTTY(node.children);
		}
	}

	void drawGraph(const ProfileThread& profileThread)
	{
		char threadName[256];
		std::snprintf(threadName, 256, "Thread %s", profileThread.threadName.c_str());

		if (ImPlot::BeginPlot(threadName, ImVec2(-1, 0), ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText))
		{
			float values[FC_PROFILE_HISTORY_MAX_SIZE];
			float max = FLT_MIN;
			for (int i = 0; i < FC_PROFILE_HISTORY_MAX_SIZE; i++)
			{
				values[i] = profileThread.history[i] * 1000.0f;
				if (values[i] > max)
					max = values[i];
			}

			ImPlot::SetupAxis(ImAxis_X1, "Frame");
			ImPlot::SetupAxis(ImAxis_Y1, "Time (ms)");
			ImPlot::SetupAxesLimits(0, FC_PROFILE_HISTORY_MAX_SIZE, 0.0f, max, ImGuiCond_Always);
			ImPlot::PlotLine(threadName, values, FC_PROFILE_HISTORY_MAX_SIZE, 1.0f, 0.0f, ImPlotLineFlags_Shaded, profileThread.historyIdx);
			ImPlot::EndPlot();
		}
	}
}
