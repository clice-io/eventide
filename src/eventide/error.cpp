#include "eventide/error.h"

#include "libuv.h"

namespace eventide {

struct uv_error_category : public std::error_category {
    const char* name() const noexcept override {
        return "libuv";
    }

    std::string message(int code) const override {
#define UV_ERROR_HANDLE(name, message)                                                             \
    case UV_##name: return message;

        switch(code) {
            UV_ERRNO_MAP(UV_ERROR_HANDLE)
            default: return "unknown error";
        }
#undef UV_ERROR_HANDLE
    }
};

std::error_code uv_error(int errc) {
    static uv_error_category category;
    return std::error_code(errc, category);
}

namespace uv_err {

const int argument_list_too_long = UV_E2BIG;
const int permission_denied = UV_EACCES;
const int address_already_in_use = UV_EADDRINUSE;
const int address_not_available = UV_EADDRNOTAVAIL;
const int address_family_not_supported = UV_EAFNOSUPPORT;
const int resource_temporarily_unavailable = UV_EAGAIN;
const int eai_address_family_not_supported = UV_EAI_ADDRFAMILY;
const int eai_temporary_failure = UV_EAI_AGAIN;
const int eai_bad_flags_value = UV_EAI_BADFLAGS;
const int eai_invalid_value_for_hints = UV_EAI_BADHINTS;
const int eai_request_canceled = UV_EAI_CANCELED;
const int eai_permanent_failure = UV_EAI_FAIL;
const int eai_family_not_supported = UV_EAI_FAMILY;
const int eai_out_of_memory = UV_EAI_MEMORY;
const int eai_no_address = UV_EAI_NODATA;
const int eai_unknown_node_or_service = UV_EAI_NONAME;
const int eai_argument_buffer_overflow = UV_EAI_OVERFLOW;
const int eai_resolved_protocol_unknown = UV_EAI_PROTOCOL;
const int eai_service_not_available_for_socket_type = UV_EAI_SERVICE;
const int eai_socket_type_not_supported = UV_EAI_SOCKTYPE;
const int connection_already_in_progress = UV_EALREADY;
const int bad_file_descriptor = UV_EBADF;
const int resource_busy_or_locked = UV_EBUSY;
const int operation_canceled = UV_ECANCELED;
const int invalid_unicode_character = UV_ECHARSET;
const int software_caused_connection_abort = UV_ECONNABORTED;
const int connection_refused = UV_ECONNREFUSED;
const int connection_reset_by_peer = UV_ECONNRESET;
const int destination_address_required = UV_EDESTADDRREQ;
const int file_already_exists = UV_EEXIST;
const int bad_address_in_system_call_argument = UV_EFAULT;
const int file_too_large = UV_EFBIG;
const int host_is_unreachable = UV_EHOSTUNREACH;
const int interrupted_system_call = UV_EINTR;
const int invalid_argument = UV_EINVAL;
const int io_error = UV_EIO;
const int socket_is_already_connected = UV_EISCONN;
const int illegal_operation_on_a_directory = UV_EISDIR;
const int too_many_symbolic_links_encountered = UV_ELOOP;
const int too_many_open_files = UV_EMFILE;
const int message_too_long = UV_EMSGSIZE;
const int name_too_long = UV_ENAMETOOLONG;
const int network_is_down = UV_ENETDOWN;
const int network_is_unreachable = UV_ENETUNREACH;
const int file_table_overflow = UV_ENFILE;
const int no_buffer_space_available = UV_ENOBUFS;
const int no_such_device = UV_ENODEV;
const int no_such_file_or_directory = UV_ENOENT;
const int not_enough_memory = UV_ENOMEM;
const int machine_is_not_on_the_network = UV_ENONET;
const int protocol_not_available = UV_ENOPROTOOPT;
const int no_space_left_on_device = UV_ENOSPC;
const int function_not_implemented = UV_ENOSYS;
const int socket_is_not_connected = UV_ENOTCONN;
const int not_a_directory = UV_ENOTDIR;
const int directory_not_empty = UV_ENOTEMPTY;
const int socket_operation_on_non_socket = UV_ENOTSOCK;
const int operation_not_supported_on_socket = UV_ENOTSUP;
const int value_too_large_for_defined_data_type = UV_EOVERFLOW;
const int operation_not_permitted = UV_EPERM;
const int broken_pipe = UV_EPIPE;
const int protocol_error = UV_EPROTO;
const int protocol_not_supported = UV_EPROTONOSUPPORT;
const int protocol_wrong_type_for_socket = UV_EPROTOTYPE;
const int result_too_large = UV_ERANGE;
const int read_only_file_system = UV_EROFS;
const int cannot_send_after_transport_endpoint_shutdown = UV_ESHUTDOWN;
const int invalid_seek = UV_ESPIPE;
const int no_such_process = UV_ESRCH;
const int connection_timed_out = UV_ETIMEDOUT;
const int text_file_is_busy = UV_ETXTBSY;
const int cross_device_link_not_permitted = UV_EXDEV;
const int unknown_error = UV_UNKNOWN;
const int end_of_file = UV_EOF;
const int no_such_device_or_address = UV_ENXIO;
const int too_many_links = UV_EMLINK;
const int host_is_down = UV_EHOSTDOWN;
const int remote_io_error = UV_EREMOTEIO;
const int inappropriate_ioctl_for_device = UV_ENOTTY;
const int inappropriate_file_type_or_format = UV_EFTYPE;
const int illegal_byte_sequence = UV_EILSEQ;
const int socket_type_not_supported = UV_ESOCKTNOSUPPORT;
const int no_data_available = UV_ENODATA;
const int protocol_driver_not_attached = UV_EUNATCH;
const int exec_format_error = UV_ENOEXEC;

}  // namespace uv_err

}  // namespace eventide
