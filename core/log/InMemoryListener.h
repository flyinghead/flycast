#pragma once

#include <list>
#include <mutex>
#include <vector>
#include <string>

#include "Log.h"
#include "LogManager.h"

class InMemoryListener : public LogListener
{
public:
	InMemoryListener(int max_lines);

	void Log(LogTypes::LOG_LEVELS, const char* msg) override;
	bool IsEnabled() const { return m_enable; }
	void SetEnable(bool enable) { m_enable = enable; }
	void Clear();
	int GetTailLineNo() const;
	std::list<std::string> GetTailLines(int start_line_no);
private:
	std::mutex m_log_lock;
	std::list<std::pair<int, std::string>> m_log;
	bool m_enable;
	int m_max_lines = 0;
	int m_next_line_no = 0;
};
