// The existing suyu-built Dynarmic library uses suyu assertion/log callbacks.
// Keep those callbacks visible and fatal instead of linking the full emulator.
#include <cstdio>
#include <cstdlib>
#include "common/logging.h"
void AssertFailSoftImpl(){std::fputs("Dynarmic assertion failed\n",stderr);std::abort();}
[[noreturn]] void AssertFatalImpl(){std::fputs("Dynarmic fatal assertion failed\n",stderr);std::abort();}
void Common::Log::FmtLogMessageImpl(Class,Level,const char*file,unsigned line,const char*fn,
                                   fmt::string_view format,const fmt::format_args&args){
    const auto message=fmt::vformat(format,args);
    std::fprintf(stderr,"oracle %s:%u %s: %s\n",file,line,fn,message.c_str());
}
