#include "capture.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace kota::zest {

namespace {

#ifdef _WIN32
int portable_dup(int fd) {
    return _dup(fd);
}

int portable_dup2(int fd1, int fd2) {
    return _dup2(fd1, fd2);
}

int portable_close(int fd) {
    return _close(fd);
}

int portable_fileno(std::FILE* f) {
    return _fileno(f);
}
#else
int portable_dup(int fd) {
    return ::dup(fd);
}

int portable_dup2(int fd1, int fd2) {
    return ::dup2(fd1, fd2);
}

int portable_close(int fd) {
    return ::close(fd);
}

int portable_fileno(std::FILE* f) {
    return ::fileno(f);
}
#endif

}  // namespace

OutputCapture::OutputCapture() {
    std::fflush(stdout);
    std::fflush(stderr);
    saved_out = portable_dup(portable_fileno(stdout));
    saved_err = portable_dup(portable_fileno(stderr));
    tmp_out = std::tmpfile();
    tmp_err = std::tmpfile();
    if(saved_out < 0 || saved_err < 0 || !tmp_out || !tmp_err) {
        return;
    }
    portable_dup2(portable_fileno(tmp_out), portable_fileno(stdout));
    portable_dup2(portable_fileno(tmp_err), portable_fileno(stderr));
    active = true;
}

OutputCapture::~OutputCapture() {
    if(active) {
        restore();
    }
    if(saved_out >= 0) {
        portable_close(saved_out);
    }
    if(saved_err >= 0) {
        portable_close(saved_err);
    }
    if(tmp_out) {
        std::fclose(tmp_out);
    }
    if(tmp_err) {
        std::fclose(tmp_err);
    }
}

CapturedOutput OutputCapture::finish() {
    if(!active) {
        return {};
    }
    restore();
    return {read_tmp(tmp_out), read_tmp(tmp_err)};
}

void OutputCapture::restore() {
    std::fflush(stdout);
    std::fflush(stderr);
    portable_dup2(saved_out, portable_fileno(stdout));
    portable_dup2(saved_err, portable_fileno(stderr));
    active = false;
}

std::string OutputCapture::read_tmp(std::FILE* f) {
    std::fseek(f, 0, SEEK_END);
    auto size = std::ftell(f);
    if(size <= 0) {
        return {};
    }
    std::fseek(f, 0, SEEK_SET);
    std::string buf(static_cast<std::size_t>(size), '\0');
    std::fread(buf.data(), 1, buf.size(), f);
    return buf;
}

}  // namespace kota::zest
