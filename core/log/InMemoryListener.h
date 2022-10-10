#pragma once

#include <list>
#include <mutex>
#include <string>

#include "Log.h"
#include "LogManager.h"

class InMemoryListener : public LogListener
{
public:
	InMemoryListener() = default;
	~InMemoryListener() override = default;

	void Log(LogTypes::LOG_LEVELS, const char* msg) override;
	void SetMaxLines(int maxLines) { m_max_lines = maxLines; }
	void Clear();
	std::list<std::string> GetLines(int start_line_no, int* tail_line_no);
private:
	std::mutex m_log_lock;
	std::list<std::pair<int, std::string>> m_log;
	int m_max_lines = 0;
	int m_line_no = 0;
};

extern InMemoryListener inMemoryListener;
