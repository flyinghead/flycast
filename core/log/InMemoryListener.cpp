#include "InMemoryListener.h"

void InMemoryListener::Log(LogTypes::LOG_LEVELS, const char* msg) {
	if (m_max_lines == 0)
		return;

	std::lock_guard<std::mutex> lk(m_log_lock);
	m_log.emplace_back(++m_line_no, msg);
	if (m_max_lines < m_log.size() && !m_log.empty()) {
		m_log.pop_front();
	}
}

void InMemoryListener::Clear() {
	std::lock_guard<std::mutex> lk(m_log_lock);
	m_log.clear();
	m_line_no = 0;
}

std::list<std::string> InMemoryListener::GetLines(int start_line_no, int* tail_line_no) {
	std::lock_guard<std::mutex> lk(m_log_lock);
	std::list<std::string> dst;
	int i = 0;
	for (auto it = m_log.rbegin(); it != m_log.rend(); ++it) {
		if (it->first < start_line_no) break;
		dst.emplace_back(it->second);
	}
	std::reverse(dst.begin(), dst.end());
	if (tail_line_no != nullptr) {
		*tail_line_no = m_log.back().first;
	}
	return dst;
}

InMemoryListener inMemoryListener;
