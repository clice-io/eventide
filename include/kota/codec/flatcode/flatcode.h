#pragma once

// Umbrella header for the flatcode codec backend.
//
// Wire format: a minimal flatbuffers-style arena format using absolute uint32
// offsets, a per-table slot-offset table, and blobs bucketed by alignment
// class. See serializer.h for the full layout description.

#include "kota/codec/flatcode/deserializer.h"
#include "kota/codec/flatcode/proxy.h"
#include "kota/codec/flatcode/serializer.h"
