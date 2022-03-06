#include <flop/Flop.h>

#include <filesystem>
#include <string>

int main(int argc, char const* argv[])
{
    std::filesystem::path base{__FILE__};
    base = base.parent_path();

    std::string reference_path = (base / "reference.png").string();
    std::string test_path      = (base / "test.png").string();
    std::string output_path    = (base / "flop_ldr.png").string();

    flop_analyze(
        reference_path.c_str(), test_path.c_str(), output_path.c_str(), nullptr);

    reference_path = (base / "reference.exr").string();
    test_path      = (base / "test.exr").string();
    output_path    = (base / "flop_hdr.png").string();

    flop_analyze_hdr(reference_path.c_str(),
                     test_path.c_str(),
                     output_path.c_str(),
                     -2.f,
                     0,
                     nullptr);

    return 0;
}
