# necrofile

Track included files using shared memory

# Requirements

 - unix

# Configuration

| Name                       | Default   | Purpose                |
|:---------------------------|:----------|:-----------------------|
| necrofile.shm_size         | 67108864  | Shared memory in bytes |
| necrofile.tcp_port         | 8282      | TCP port to listen on  |
| necrofile.server_address   | 127.0.0.1 | Bind address           |
| necrofile.exclude_patterns | ""        | A comma-separated list of strings used to exclude paths that match any of the specified patterns. |
| necrofile.trim_patterns    | ""        | A comma-separated list of strings used to trim the paths up to and including any of the specified patterns. |

# Communication

Both TCP server and function `necrofiles()` return a JSON array of paths that were included.