/* Minimal HTTP stubs for head-less test runner on macOS.
 * They satisfy the linker but do not perform any IO. */
#include <string>
#include <vector>
#include <cstdint>
namespace http {
using u8 = uint8_t;
static bool inited = false;
bool init() { inited = true; return true; }
void term() { inited = false; }
int get(const std::string&, std::vector<u8>&, std::string&) { return 404; }
int get(const std::string&, std::vector<u8>&) { return 404; }
int post(const std::string&, const char*, const char*, std::vector<u8>&) { return 500; }
struct PostField { std::string name; std::string value; std::string contentType; };
int post(const std::string&, const std::vector<PostField>&) { return 500; }
}
