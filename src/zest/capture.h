#pragma once

#include <cstdio>
#include <string>

namespace kota::zest {

struct CapturedOutput {
    std::string out;
    std::string err;
};

class OutputCapture {
public:
    OutputCapture();
    ~OutputCapture();

    OutputCapture(const OutputCapture&) = delete;
    OutputCapture& operator=(const OutputCapture&) = delete;

    CapturedOutput finish();

private:
    void restore();
    static std::string read_tmp(std::FILE* f);

    int saved_out = -1;
    int saved_err = -1;
    std::FILE* tmp_out = nullptr;
    std::FILE* tmp_err = nullptr;
    bool active = false;
};

}  // namespace kota::zest
