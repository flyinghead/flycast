// Thin wrapper to provide a standard C main() symbol for the test executable
// and forward to the existing SDL_main implemented in gtest_bootstrap.cpp.
extern "C" int SDL_main(int argc, char* argv[]);

extern "C" int main(int argc, char* argv[])
{
    return SDL_main(argc, argv);
}
