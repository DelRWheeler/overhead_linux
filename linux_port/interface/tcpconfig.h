//////////////////////////////////////////////////////////////////////
// tcpconfig.h: TCP/IP configuration for Interface.exe
//
// This replaces Named Pipe configuration with TCP/IP settings
//////////////////////////////////////////////////////////////////////

#ifndef _TCPCONFIG_H_
#define _TCPCONFIG_H_

// TCP Server configuration (for receiving messages from host/API)
#define TCP_SERVER_PORT         5000    // Port to listen on for incoming connections
#define TCP_SERVER_BACKLOG      6       // Max pending connections (matches INSTANCES)
#define TCP_MAX_CLIENTS         6       // Max concurrent client connections

// TCP Client configuration (for sending messages to other RTX controllers)
#define TCP_CLIENT_BASE_PORT    5000    // Base port for connecting to remote controllers
#define TCP_CONNECT_TIMEOUT_MS  5000    // Connection timeout in milliseconds
#define TCP_SEND_TIMEOUT_MS     1000    // Send timeout in milliseconds
#define TCP_RECV_TIMEOUT_MS     1000    // Receive timeout in milliseconds

// Message framing
// TCP is stream-based, so we need length-prefix framing
// Format: [4-byte length][message data]
#define TCP_LENGTH_PREFIX_SIZE  4

// Buffer sizes
#define TCP_RECV_BUFFER_SIZE    MAX_APP_MSG_SIZE
#define TCP_SEND_BUFFER_SIZE    MAX_APP_MSG_SIZE

// Reconnect settings
#define TCP_RECONNECT_INTERVAL_MS  1000 // Time between reconnect attempts

// Socket options
#define TCP_KEEPALIVE_ENABLE    1       // Enable TCP keepalive
#define TCP_NODELAY_ENABLE      1       // Disable Nagle algorithm for low latency

// Configuration structure for remote hosts
// This replaces the pipe_path strings in IsysLineSettings
// The IP addresses will be stored in shared memory similar to pipe paths
typedef struct {
    char ip_address[16];    // IP address string (e.g., "192.168.100.20")
    USHORT port;            // TCP port number
    BOOL active;            // Is this connection active?
} TCP_HOST_CONFIG;

// Array indices (same as pipe indices)
// Index 0-3: Remote RTX controllers (Line 1-4)
// Index 4: Host (Delphi app / API server)
#define TCP_HOST_INDEX          (HOST_ID - 1)   // Index 4 for host

#endif // _TCPCONFIG_H_
