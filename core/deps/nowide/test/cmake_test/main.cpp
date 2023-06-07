//
// Copyright (c) 2019-2021 Alexander Grund
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <nowide/args.hpp>
#include <nowide/convert.hpp>

int main(int argc, char** argv, char** env)
{
    nowide::args _(argc, argv, env);
    if(argc < 1)
        return 1;
    if(nowide::narrow(nowide::widen(argv[0])) != argv[0])
        return 1;
    return 0;
}
