#pragma once

#include <expected>
#include <system_error>

namespace eventide {

std::error_code uv_error(int errc);

template <typename T>
using result = std::expected<T, std::error_code>;

namespace uv_err {

// argument list too long
extern const int argument_list_too_long;
// permission denied
extern const int permission_denied;
// address already in use
extern const int address_already_in_use;
// address not available
extern const int address_not_available;
// address family not supported
extern const int address_family_not_supported;
// resource temporarily unavailable
extern const int resource_temporarily_unavailable;
// address family not supported
extern const int eai_address_family_not_supported;
// temporary failure
extern const int eai_temporary_failure;
// bad ai_flags value
extern const int eai_bad_flags_value;
// invalid value for hints
extern const int eai_invalid_value_for_hints;
// request canceled
extern const int eai_request_canceled;
// permanent failure
extern const int eai_permanent_failure;
// ai_family not supported
extern const int eai_family_not_supported;
// out of memory
extern const int eai_out_of_memory;
// no address
extern const int eai_no_address;
// unknown node or service
extern const int eai_unknown_node_or_service;
// argument buffer overflow
extern const int eai_argument_buffer_overflow;
// resolved protocol is unknown
extern const int eai_resolved_protocol_unknown;
// service not available for socket type
extern const int eai_service_not_available_for_socket_type;
// socket type not supported
extern const int eai_socket_type_not_supported;
// connection already in progress
extern const int connection_already_in_progress;
// bad file descriptor
extern const int bad_file_descriptor;
// resource busy or locked
extern const int resource_busy_or_locked;
// operation canceled
extern const int operation_canceled;
// invalid Unicode character
extern const int invalid_unicode_character;
// software caused connection abort
extern const int software_caused_connection_abort;
// connection refused
extern const int connection_refused;
// connection reset by peer
extern const int connection_reset_by_peer;
// destination address required
extern const int destination_address_required;
// file already exists
extern const int file_already_exists;
// bad address in system call argument
extern const int bad_address_in_system_call_argument;
// file too large
extern const int file_too_large;
// host is unreachable
extern const int host_is_unreachable;
// interrupted system call
extern const int interrupted_system_call;
// invalid argument
extern const int invalid_argument;
// i/o error
extern const int io_error;
// socket is already connected
extern const int socket_is_already_connected;
// illegal operation on a directory
extern const int illegal_operation_on_a_directory;
// too many symbolic links encountered
extern const int too_many_symbolic_links_encountered;
// too many open files
extern const int too_many_open_files;
// message too long
extern const int message_too_long;
// name too long
extern const int name_too_long;
// network is down
extern const int network_is_down;
// network is unreachable
extern const int network_is_unreachable;
// file table overflow
extern const int file_table_overflow;
// no buffer space available
extern const int no_buffer_space_available;
// no such device
extern const int no_such_device;
// no such file or directory
extern const int no_such_file_or_directory;
// not enough memory
extern const int not_enough_memory;
// machine is not on the network
extern const int machine_is_not_on_the_network;
// protocol not available
extern const int protocol_not_available;
// no space left on device
extern const int no_space_left_on_device;
// function not implemented
extern const int function_not_implemented;
// socket is not connected
extern const int socket_is_not_connected;
// not a directory
extern const int not_a_directory;
// directory not empty
extern const int directory_not_empty;
// socket operation on non-socket
extern const int socket_operation_on_non_socket;
// operation not supported on socket
extern const int operation_not_supported_on_socket;
// value too large for defined data type
extern const int value_too_large_for_defined_data_type;
// operation not permitted
extern const int operation_not_permitted;
// broken pipe
extern const int broken_pipe;
// protocol error
extern const int protocol_error;
// protocol not supported
extern const int protocol_not_supported;
// protocol wrong type for socket
extern const int protocol_wrong_type_for_socket;
// result too large
extern const int result_too_large;
// read-only file system
extern const int read_only_file_system;
// cannot send after transport endpoint shutdown
extern const int cannot_send_after_transport_endpoint_shutdown;
// invalid seek
extern const int invalid_seek;
// no such process
extern const int no_such_process;
// connection timed out
extern const int connection_timed_out;
// text file is busy
extern const int text_file_is_busy;
// cross-device link not permitted
extern const int cross_device_link_not_permitted;
// unknown error
extern const int unknown_error;
// end of file
extern const int end_of_file;
// no such device or address
extern const int no_such_device_or_address;
// too many links
extern const int too_many_links;
// host is down
extern const int host_is_down;
// remote I/O error
extern const int remote_io_error;
// inappropriate ioctl for device
extern const int inappropriate_ioctl_for_device;
// inappropriate file type or format
extern const int inappropriate_file_type_or_format;
// illegal byte sequence
extern const int illegal_byte_sequence;
// socket type not supported
extern const int socket_type_not_supported;
// no data available
extern const int no_data_available;
// protocol driver not attached
extern const int protocol_driver_not_attached;
// exec format error
extern const int exec_format_error;

}  // namespace uv_err

struct cancellation_t {};

template <typename T>
constexpr bool is_cancellation_t = false;

template <typename T>
constexpr bool is_cancellation_t<std::expected<T, cancellation_t>> = true;

template <typename T>
class task;

template <typename T>
using ctask = task<std::expected<T, cancellation_t>>;

}  // namespace eventide
