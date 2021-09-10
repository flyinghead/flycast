class Sdl2 < Formula
  desc "Low-level access to audio, keyboard, mouse, joystick, and graphics"
  homepage "https://www.libsdl.org/"
  url "https://libsdl.org/release/SDL2-2.0.16.tar.gz"
  sha256 "65be9ff6004034b5b2ce9927b5a4db1814930f169c4b2dae0a1e4697075f287b"
  license "Zlib"
  revision 1
  env :std

  livecheck do
    url "https://www.libsdl.org/download-2.0.php"
    regex(/SDL2[._-]v?(\d+(?:\.\d+)*)/i)
  end

  bottle do
    sha256 cellar: :any, arm64_big_sur: "6adac3ca2899ab923427b9b9322c8a4a412485ac7fe6448e276b4aae598f7a49"
    sha256 cellar: :any, big_sur:       "71fe247bc197133b02186fac4e8f296d7f457a9507e0c77357b1069e5ee2ca61"
    sha256 cellar: :any, catalina:      "4634185a35d9fc37c8fc07f884e45e7e2fbaa3fdec615171e647a9e02c395bd4"
    sha256 cellar: :any, mojave:        "9966890d7d39147e75e92d6a7390ef5fb2f043b08f913e751638bdeef8c1c220"
  end

  head do
    url "https://github.com/libsdl-org/SDL.git", branch: "main"

    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
  end

  on_linux do
    depends_on "pkg-config" => :build
  end

  def install
    # Delete default flags for cross compiling
    ENV.delete('CFLAGS')
    ENV.delete('CXXFLAGS')
    ENV.delete('CPPFLAGS')
    ENV.delete('LDFLAGS')
    ENV.delete('CMAKE_PREFIX_PATH')
    ENV.delete('CMAKE_FRAMEWORK_PATH')
    ENV.delete('CPATH')
    sdkpath = %x[xcode-select -p]
    sdkpath = sdkpath.chomp + "/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    ENV['SDKROOT'] = sdkpath
    ENV['CPP'] = "/usr/bin/cpp"
    ENV['CXXCPP'] = "/usr/bin/cpp"
    
    # we have to do this because most build scripts assume that all SDL modules
    # are installed to the same prefix. Consequently SDL stuff cannot be
    # keg-only but I doubt that will be needed.
    inreplace %w[sdl2.pc.in sdl2-config.in], "@prefix@", HOMEBREW_PREFIX

    system "./autogen.sh" if build.head?

    args = %W[--prefix=#{prefix} --without-x --enable-hidapi]
    args << "CFLAGS=-mmacosx-version-min=10.9"
    args << "CXXFLAGS=-mmacosx-version-min=10.9"
    args << "CC=gcc -isysroot #{sdkpath} -arch arm64 -arch x86_64"
    args << "CXX=g++ -isysroot #{sdkpath} -arch arm64 -arch x86_64"
    system "./configure", *args
    system "make", "install"
  end

  test do
    system bin/"sdl2-config", "--version"
  end
end
