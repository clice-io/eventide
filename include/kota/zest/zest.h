#pragma once

#include "kota/zest/assert/check.h"
#include "kota/zest/assert/trace.h"
#include "kota/zest/macro.h"
#include "kota/zest/runner/run.h"
#include "kota/zest/runner/suite.h"
#include "kota/zest/snapshot/snapshot.h"

// Matches the guard "kota/zest/macro.h" uses for the snapshot-JSON macros, which
// name ::kota::codec::json but cannot include it themselves.
#if __has_include("kota/codec/json/json.h")
#include "kota/codec/json/json.h"
#endif
