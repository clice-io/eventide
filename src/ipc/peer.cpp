#include "eventide/ipc/peer.h"

namespace eventide::ipc {

#if ENABLE_JSON
template class Peer<JsonCodec>;
#endif
template class Peer<BincodeCodec>;

}  // namespace eventide::ipc
