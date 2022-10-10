#include "InMemoryListener.h"

InMemoryListener::InMemoryListener(int max_lines) {
	SetEnable(true);
	m_max_lines = max_lines;
	m_next_line_no = 1;
}

void InMemoryListener::Log(LogTypes::LOG_LEVELS, const char* msg) {
	if (!IsEnabled() || m_max_lines == 0)
		return;

	std::lock_guard<std::mutex> lk(m_log_lock);
	m_log.emplace_back(m_next_line_no++, msg);
	if (m_max_lines < m_log.size() && !m_log.empty()) {
		m_log.pop_front();
	}
}

void InMemoryListener::Clear() {
	std::lock_guard<std::mutex> lk(m_log_lock);
	m_log.clear();
}

int InMemoryListener::GetTailLineNo() const {
	if (m_log.empty()) return -1;
	return m_log.back().first;
}

std::list<std::string> InMemoryListener::GetTailLines(int start_line_no) {
	std::lock_guard<std::mutex> lk(m_log_lock);
	std::list<std::string> dst;
	int i = 0;
	for (auto it = m_log.rbegin(); it != m_log.rend(); ++it) {
		if (it->first < start_line_no) break;
		dst.emplace_back(it->second);
	}
	std::reverse(dst.begin(), dst.end());
	return dst;
}
