class LibpngFlycast < Formula
  desc "Library for manipulating PNG images"
  homepage "http://www.libpng.org/pub/png/libpng.html"
  url "https://downloads.sourceforge.net/project/libpng/libpng16/1.6.55/libpng-1.6.55.tar.gz"
  sha256 "4b0abab6d219e95690ebe4db7fc9aa95f4006c83baaa022373c0c8442271283d"
  license "libpng-2.0"

  depends_on "cmake" => :build
  uses_from_macos "zlib"

  # Keg-only to avoid conflict with standard libpng and fix CI resolution
  keg_only "it is a static build for Flycast"

  option "with-intel", "Build only the Intel macOS library"
  option "with-arm", "Build only the Apple Silicon macOS library"

  def install
    odie "Choose either --with-intel or --with-arm" if build.with?("intel") && build.with?("arm")
    architectures = build.with?("intel") ? "x86_64" : (build.with?("arm") ? "arm64" : "arm64;x86_64")
    deployment_target = build.with?("arm") ? "11.0" : "10.15"
    ENV.permit_arch_flags
    args = std_cmake_args + [
      "-DCMAKE_OSX_ARCHITECTURES=#{architectures}",
      "-DCMAKE_OSX_DEPLOYMENT_TARGET=#{deployment_target}",
      "-DPNG_SHARED=OFF",
      "-DPNG_TESTS=OFF",
      "-DPNG_ARM_NEON=off",
      # Universal builds skip libpng's per-architecture NEON definitions.
      # Keep its ARM sources and callers consistently disabled in both slices.
      "-DCMAKE_C_FLAGS=-DPNG_ARM_NEON_OPT=0",
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
