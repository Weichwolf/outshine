class Meshoptimizer < Formula
  desc "Mesh optimization library for vertex and index buffers"
  homepage "https://github.com/zeux/meshoptimizer"
  url "https://github.com/zeux/meshoptimizer/archive/refs/tags/v1.3.tar.gz"
  sha256 "ca4f47d451201593904b9e3067fa54b65db94bc34ab5173864950862b748a145"
  license "MIT"

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args,
           "-DMESHOPT_BUILD_SHARED_LIBS=ON", "-DMESHOPT_STABLE_EXPORTS=ON"
    system "cmake", "--build", "build", "--parallel", "2"
    system "cmake", "--install", "build"
  end

  test do
    (testpath/"test.cpp").write <<~CPP
      #include <meshoptimizer.h>
      int main() {
        const unsigned int indices[] = {0, 1, 2};
        unsigned int result[3];
        meshopt_optimizeVertexCache(result, indices, 3, 3);
        return result[0] != 0 || result[1] != 1 || result[2] != 2;
      }
    CPP
    system ENV.cxx, "test.cpp", "-I#{include}", "-L#{lib}", "-lmeshoptimizer", "-o", "test"
    system "./test"
  end
end
