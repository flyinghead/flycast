/*
	normalize prefix
*/
#include <string>
#include <set>
#include <iostream>
#include <memory.h>

typedef unsigned char uint8;

std::string normalize(const std::string& line)
{
	static const char tbl[][3] = { "66", "67", "F2", "F3" };
	size_t tblNum = sizeof(tbl) / sizeof(tbl[0]);
	typedef std::set<std::string> StringSet;
	StringSet suf;

	size_t pos = 0;
	for (; pos < line.size(); pos += 2) {
		bool found = false;
		for (size_t i = 0; i < tblNum; i++) {
			if (::memcmp(&line[pos], tbl[i], 2) == 0) {
				found = true;
				suf.insert(tbl[i]);
				break;
			}
		}
		if (!found) break;
	}
	std::string ret;
	for (StringSet::const_iterator i = suf.begin(), e = suf.end(); i != e; ++i) {
		ret += *i;
	}
	ret += &line[pos];
	return ret;
}

int main()
{
	std::string line;
	while (std::getline(std::cin, line)) {
		std::string normalizedLine = normalize(line);
		std::cout << normalizedLine << '\n';//std::endl;
	}
}
