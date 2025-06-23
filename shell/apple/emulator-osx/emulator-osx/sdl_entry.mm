// Simple SDL entry point that forwards to the actual Flycast SDL main.
// This exists so the GUI app bundle does not pull in GoogleTest symbols.

extern "C" int FlycastSDLMain(int argc, char* argv[]);

extern "C" int SDL_main(int argc, char* argv[])
{
    return FlycastSDLMain(argc, argv);
}
