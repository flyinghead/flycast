# Install

The following files are necessary. Please add the path to your compile directory.

* xbyak.h
* xbyak_mnemonic.h
* xbyak_util.h

Linux:
```
make install
```

These files are copied into `/usr/local/include/xbyak`.

# Building xbyak - Using vcpkg

You can download and install xbyak using the [vcpkg](https://github.com/Microsoft/vcpkg) dependency manager:

    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    ./vcpkg integrate install
    ./vcpkg install xbyak

The xbyak port in vcpkg is kept up to date by Microsoft team members and community contributors. If the version is out of date, please [create an issue or pull request](https://github.com/Microsoft/vcpkg) on the vcpkg repository.
