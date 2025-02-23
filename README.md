# necrofile

Track included files using shared memory

# Requirements

 - unix

# Configuration

| Name                       | Default   | Purpose                |
|:---------------------------|:----------|:-----------------------|
| necrofile.redis_host       | 127.0.0.1 | Redis server host      |
| necrofile.redis_port       | 6379      | Redis server port      |
| necrofile.server_address   | 127.0.0.1 | Bind address           |
| necrofile.tcp_port         | 8282      | TCP port to listen on  |
| necrofile.exclude_patterns | ""        | A comma-separated list of strings used to exclude paths that match any of the specified patterns. |
| necrofile.trim_patterns    | ""        | A comma-separated list of strings used to trim the paths up to and including any of the specified patterns. |

# Communication

Both TCP server and function `necrofiles()` return a JSON array of paths that were included.