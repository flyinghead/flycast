class Libomp < Formula
  desc "LLVM's OpenMP runtime library"
  homepage "https://openmp.llvm.org/"
  url "https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.0/openmp-21.1.0.src.tar.xz"
  sha256 "60d6c4d2019d546aefc029c9b2c4006dc78419dba4cfd0a376ae025be8ff9778"
  license "MIT"

  livecheck do
    url :stable
    regex(/^llvmorg[._-]v?(\d+(?:\.\d+)+)$/i)
  end

  bottle do
    sha256 cellar: :any,                 arm64_sequoia: "0ceba5785dc521e24a27be3a17f82d8712fc0bbf31350b89f20df1cccfc7a37f"
    sha256 cellar: :any,                 arm64_sonoma:  "efcf5b3a7b8c8e93afc12727321ec4ca1485a516603c4a59abd27131973cbeb5"
    sha256 cellar: :any,                 arm64_ventura: "898cb346422ccc067020c808b311489470df91b35ff5371948b58e0e51200b53"
    sha256 cellar: :any,                 sonoma:        "1ac8d9e88130a73308ec32dd6da011fd2433ff031b7a11398293ba36ab6c930d"
    sha256 cellar: :any,                 ventura:       "f92736c1181011c5a7de2095abffaa56f55e2593349d835d17ee9c9ffe6041d6"
    sha256 cellar: :any_skip_relocation, arm64_linux:   "8d84032bbe43b9686016039b2ab3e7bb92122c0c2e597c7a97b16c215e752d00"
    sha256 cellar: :any_skip_relocation, x86_64_linux:  "41f5e91b9b86f0e6ed64596c20a8173aa152c36348f38645b5d9beb51b7a0d4f"
  end

  # Ref: https://github.com/Homebrew/homebrew-core/issues/112107
  keg_only "it can override GCC headers and result in broken builds"

  # depends_on "cmake" => :build     #Flycast
  depends_on "lit" => :build
  uses_from_macos "llvm" => :build

  on_linux do
    depends_on "python@3.13"
  end

  resource "cmake" do
    url "https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.0/cmake-21.1.0.src.tar.xz"
    sha256 "528347c84c3571d9d387b825ef8b07c7ad93e9437243c32173838439c3b6028f"

    livecheck do
      formula :parent
    end
  end

  def install
    # Manually specify the path to the local pinned cmake     #Flycast
    ENV.prepend_path "PATH", Formula["local/pinned/cmake"].opt_bin

    odie "cmake resource needs to be updated" if version != resource("cmake").version

    (buildpath/"src").install buildpath.children
    (buildpath/"cmake").install resource("cmake")

    # Disable LIBOMP_INSTALL_ALIASES, otherwise the library is installed as
    # libgomp alias which can conflict with GCC's libgomp.
    args = ["-DLIBOMP_INSTALL_ALIASES=OFF"]
    args << "-DOPENMP_ENABLE_LIBOMPTARGET=OFF" if OS.linux?

    # Build universal binary     #Flycast
    ENV.permit_arch_flags
    ENV.runtime_cpu_detection
    args << "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
    args << "-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15"

    # system "cmake", "-S", "src", "-B", "build/shared", *std_cmake_args, *args
    # system "cmake", "--build", "build/shared"
    # system "cmake", "--install", "build/shared"

    system "cmake", "-S", "src", "-B", "build/static",
                    "-DLIBOMP_ENABLE_SHARED=OFF",
                    *std_cmake_args, *args
    system "cmake", "--build", "build/static"
    system "cmake", "--install", "build/static"
  end

  test do
    (testpath/"test.cpp").write <<~CPP
      #include <omp.h>
      #include <array>
      int main (int argc, char** argv) {
        std::array<size_t,2> arr = {0,0};
        #pragma omp parallel num_threads(2)
        {
            size_t tid = omp_get_thread_num();
            arr.at(tid) = tid + 1;
        }
        if(arr.at(0) == 1 && arr.at(1) == 2)
            return 0;
        else
            return 1;
      }
    CPP
    system ENV.cxx, "-Werror", "-Xpreprocessor", "-fopenmp", "test.cpp", "-std=c++11",
                    "-I#{include}", "-L#{lib}", "-lomp", "-o", "test"
    system "./test"
  end
end