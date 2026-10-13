class LibpngFlycast < Formula
  desc "Library for manipulating PNG images"
  homepage "http://www.libpng.org/pub/png/libpng.html"
  url "https://downloads.sourceforge.net/project/libpng/libpng16/1.6.55/libpng-1.6.55.tar.gz"
  sha256 "4b0abab6d219e95690ebe4db7fc9aa95f4006c83baaa022373c0c8442271283d"
  license "libpng-2.0"

  depends_on "cmake" => :build
  uses_from_macos "zlib"

  # Keg-only to avoid conflict with standard libpng and fix CI resolution
  keg_only "it is a universal static build for Flycast"

  def install
    # Build universal binary     #Flycast
    ENV.permit_arch_flags
    args = std_cmake_args + [
      "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64",
      "-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15",
      "-DPNG_SHARED=OFF",
      "-DPNG_TESTS=OFF",
      "-DPNG_ARM_NEON=off",
      "-DPNG_FRAMEWORK=OFF"
    ]

    system "cmake", "-S", ".", "-B", "build", *args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    (testpath/"test.c").write <<~C
      #include <png.h>
      int main() {
        fprintf(stderr, "libpng version: %s\\n", PNG_LIBPNG_VER_STRING);
        return 0;
      }
    C
    system ENV.cc, "test.c", "-I#{include}", "-L#{lib}", "-lpng", "-o", "test"
    system "./test"
  end
end
