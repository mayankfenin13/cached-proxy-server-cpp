# HTTP Proxy Server

A multi-threaded HTTP proxy server implemented in C++ with LRU caching capabilities and semaphore-based connection management. This project demonstrates concurrent programming, network socket programming, and efficient caching mechanisms.

## 🚀 Features

- **Multi-threaded Architecture**: Handles multiple client connections concurrently using pthreads
- **LRU Caching**: Implements Least Recently Used caching to improve response times for frequently accessed resources
- **Semaphore-based Connection Management**: Controls the maximum number of concurrent connections using POSIX semaphores
- **HTTP Request Parsing**: Custom HTTP request parser for handling client requests
- **Error Handling**: Comprehensive error responses with appropriate HTTP status codes
- **Thread-safe Operations**: Mutex-protected cache operations to ensure data consistency

## 📁 Project Structure

```
├── server.cpp                     # Main proxy server implementation with LRU cache
request parsing library header
├── proxy_parse.cpp               # HTTP request parsing library implementation
├── LRUCache.h                     # LRU Cache data structure implementation
└── README.md                      # Project documentation
```

## 🛠️ Prerequisites

- **Operating System**: Linux (tested on Ubuntu/Debian)
- **Compiler**: g++ with C++11 support or later
- **Libraries**: pthread, semaphore

## 📦 Installation & Compilation

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd OS
   ```

2. **Compile the proxy server**:
   ```bash
   g++ -o proxy_server server.cpp -lpthread
   ```

## 🚀 Usage

### Starting the Proxy Server

```bash
# Start the server on default port 8080
./proxy_server

# The server will display:
# Client connected!
# IP Address: <client_ip>
# Port: <client_port>
```

### Configuring Your Browser

1. **Firefox**:
   - Go to Settings → Network Settings → Manual proxy configuration
   - HTTP Proxy: `localhost`, Port: `8080`

2. **Chrome**:
   - Use command line: `google-chrome --proxy-server="localhost:8080"`

3. **curl**:
   ```bash
   curl --proxy localhost:8080 http://example.com
   ```

### Testing the Proxy

```bash
# Test with curl
curl --proxy localhost:8080/http://httpbin.org/get

# Test caching by making the same request multiple times
curl --proxy localhost:8080/http://example.com
curl --proxy localhost:8080/http://example.com  # This should be served from cache
```

## 🏗️ Architecture

### Core Components

1. **Thread Management**:
   - Semaphore controls maximum concurrent connections (MAX_CLIENTS = 10)
   - Each client connection spawns a new thread
   - Thread-safe operations using mutexes

2. **HTTP Request Processing**:
   - Parses incoming HTTP requests using custom parser
   - Validates request format and method (only GET supported)
   - Forwards requests to target servers

3. **LRU Cache**:
   - Stores server responses for faster subsequent requests
   - Implements Least Recently Used eviction policy
   - Thread-safe operations with mutex protection

4. **Network Communication**:
   - Handles client-proxy and proxy-server connections
   - Robust error handling and connection management
   - Proper socket cleanup and resource management

### Data Flow

```
Client → Proxy Server → Check Cache → [Cache Hit] → Return Cached Response
                     ↓
                [Cache Miss] → Forward to Target Server → Cache Response → Return to Client
```

## 🔧 Configuration

### Server Parameters

- **Port**: Default 8080 (configurable in main function)
- **Max Clients**: 10 concurrent connections
- **Cache Size**: 100 entries (configurable in LRUCache constructor)
- **Buffer Size**: 4096 bytes

### Modifying Configuration

```cpp
#define MAX_CLIENTS 10    // Maximum concurrent connections
#define MAX_BYTES 4096    // Buffer size for data transfer

// In main():
int PORT = 8080;          // Server port
cache = new LRUCache(100); // Cache capacity
```

## 🚨 Error Handling

The proxy server handles various error conditions:

- **400 Bad Request**: Invalid HTTP request format
- **404 Not Found**: Resource not found
- **500 Internal Server Error**: Server-side errors
- **502 Bad Gateway**: Cannot connect to target server

## 📈 Performance Features

- **Caching**: LRU cache reduces latency for repeated requests
- **Connection Pooling**: Semaphore-based connection management
- **Concurrent Processing**: Multi-threaded architecture for handling multiple clients
- **Efficient Parsing**: Custom HTTP parser optimized for proxy operations

## 🔒 Security Considerations

- Only HTTP GET requests are supported
- Connection header is set to "close" for security
- Input validation and bounds checking
- Proper resource cleanup to prevent memory leaks

## 👥 Authors

- Harsh Bansal - [harsh@harshbansal.in](mailto:harsh@harshbansal.in)
