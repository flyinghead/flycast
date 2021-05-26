#include <iostream>
#include <fstream>
#include <string>
#include <set>

typedef std::set<std::string> StrSet;

int main()
{
	StrSet ss;
	std::string line;
	while (std::getline(std::cin, line)) {
		if (!line.empty() && line[line.size() - 1] == '\n') {
			line.resize(line.size() - 1);
		}
		if (!line.empty()) {
			ss.insert(line);
		}
	}
	for (StrSet::const_iterator i = ss.begin(), ie = ss.end(); i != ie; ++i) {
		std::cout << *i << std::endl;
	}
}
