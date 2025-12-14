/**
 * @file mesh_routing.h
 * @brief Mesh network routing for sensor-to-sensor relay
 * 
 * Implements AODV-like routing with:
 * - Route discovery via flooding
 * - Routing table with hop counts
 * - Packet forwarding
 * - Self-healing network topology
 * 
 * @version 3.0.0
 * @date 2025-12-13
 */

#ifndef MESH_ROUTING_H
#define MESH_ROUTING_H

#include <Arduino.h>
#include <vector>
#include "data_types.h"

// Maximum hops before packet is dropped
#define MAX_HOPS 5

// Maximum routing table entries
#define MAX_ROUTES 32

// Route timeout (10 minutes)
#define ROUTE_TIMEOUT_MS 600000

// Route discovery timeout (5 seconds)
#define ROUTE_DISCOVERY_TIMEOUT_MS 5000

// Neighbor beacon interval (30 seconds)
#define NEIGHBOR_BEACON_INTERVAL_MS 30000

/**
 * @brief Mesh packet types
 */
enum MeshPacketType {
    MESH_DATA = 0,              // User data packet
    MESH_ROUTE_REQUEST = 1,     // Route discovery request (RREQ)
    MESH_ROUTE_REPLY = 2,       // Route discovery reply (RREP)
    MESH_ROUTE_ERROR = 3,       // Route error notification
    MESH_NEIGHBOR_BEACON = 4    // Neighbor discovery beacon
};

/**
 * @brief Mesh routing header (prepended to all packets)
 */
struct MeshHeader {
    uint8_t packetType;         // MeshPacketType
    uint8_t sourceId;           // Original sender
    uint8_t destId;             // Final destination (255 = broadcast)
    uint8_t nextHop;            // Next hop in route
    uint8_t prevHop;            // Previous hop (who forwarded to us)
    uint8_t hopCount;           // Number of hops so far
    uint8_t ttl;                // Time to live (remaining hops)
    uint16_t sequenceNum;       // Unique packet identifier
} __attribute__((packed));

/**
 * @brief Route discovery request packet
 */
struct RouteRequest {
    MeshHeader header;
    uint8_t destId;             // Who we're looking for
    uint8_t hopCount;           // Hops from source
    uint16_t requestId;         // Unique RREQ identifier
} __attribute__((packed));

/**
 * @brief Route discovery reply packet
 */
struct RouteReply {
    MeshHeader header;
    uint8_t destId;             // Destination of original request
    uint8_t hopCount;           // Hops to destination
    uint16_t requestId;         // Matching RREQ identifier
} __attribute__((packed));

/**
 * @brief Neighbor beacon packet
 */
struct NeighborBeacon {
    MeshHeader header;
    uint8_t neighborId;         // Who is sending beacon
    int16_t rssi;               // Signal strength
    uint8_t hopDistance;        // Hops to base station
} __attribute__((packed));

/**
 * @brief Routing table entry
 */
struct RouteEntry {
    uint8_t destId;             // Destination node ID
    uint8_t nextHop;            // Next hop to reach destination
    uint8_t hopCount;           // Number of hops to destination
    uint32_t lastUsed;          // Timestamp of last use
    int16_t linkQuality;        // RSSI or similar metric
    bool isValid;               // Entry is active
};

/**
 * @brief Neighbor information
 */
struct Neighbor {
    uint8_t nodeId;             // Neighbor node ID
    int16_t rssi;               // Signal strength
    uint32_t lastSeen;          // Last beacon timestamp
    uint8_t hopDistance;        // Hops to base station
    bool isActive;              // Neighbor is reachable
};

/**
 * @brief Mesh routing manager
 * 
 * Handles all mesh networking operations:
 * - Route discovery and maintenance
 * - Packet forwarding
 * - Neighbor management
 * - Network topology tracking
 */
class MeshRouter {
public:
    MeshRouter();
    
    // Initialization
    void begin(uint8_t nodeId, bool isBaseStation = false);
    
    // Packet handling
    bool sendPacket(uint8_t destId, uint8_t* payload, size_t payloadSize);
    bool forwardPacket(MeshHeader* header, uint8_t* payload, size_t payloadSize);
    void processReceivedPacket(uint8_t* packet, size_t packetSize, int16_t rssi);
    
    // Route management
    bool hasRouteTo(uint8_t destId);
    RouteEntry* getRoute(uint8_t destId);
    void addRoute(uint8_t destId, uint8_t nextHop, uint8_t hopCount);
    void removeRoute(uint8_t destId);
    void updateRoute(uint8_t destId, uint8_t nextHop, uint8_t hopCount, int16_t rssi);
    void cleanupExpiredRoutes();
    
    // Route discovery
    bool discoverRoute(uint8_t destId);
    void sendRouteRequest(uint8_t destId);
    void processRouteRequest(RouteRequest* rreq, int16_t rssi);
    void processRouteReply(RouteReply* rrep);
    
    // Neighbor management
    void sendNeighborBeacon();
    void processNeighborBeacon(NeighborBeacon* beacon, int16_t rssi);
    Neighbor* getNeighbor(uint8_t nodeId);
    void cleanupExpiredNeighbors();
    std::vector<Neighbor>& getNeighbors() { return neighbors; }
    
    // Network health
    void loop();  // Call from main loop
    uint8_t getNeighborCount() const;
    uint8_t getRouteCount() const;
    float getNetworkHealth() const;
    
    // Status
    void printRoutingTable();
    void printNeighbors();
    String getNetworkTopologyJSON();
    
    // Configuration
    void setForwardingEnabled(bool enabled) { forwardingEnabled = enabled; }
    bool isForwardingEnabled() const { return forwardingEnabled; }
    
private:
    uint8_t nodeId;
    bool isBaseStation;
    bool forwardingEnabled;
    uint16_t sequenceNumber;
    uint16_t requestId;
    
    std::vector<RouteEntry> routingTable;
    std::vector<Neighbor> neighbors;
    
    uint32_t lastBeaconTime;
    uint32_t lastRouteCleanup;
    uint32_t lastNeighborCleanup;
    
    // Statistics
    uint32_t packetsForwarded;
    uint32_t packetsDropped;
    uint32_t routeDiscoveries;
    
    // Helper functions
    bool isPacketDuplicate(uint8_t sourceId, uint16_t seqNum);
    void updateSeenPackets(uint8_t sourceId, uint16_t seqNum);
    uint16_t getNextSequenceNumber();
    uint16_t getNextRequestId();
    
    // Duplicate packet tracking (simple cache)
    struct PacketId {
        uint8_t sourceId;
        uint16_t seqNum;
        uint32_t timestamp;
    };
    std::vector<PacketId> seenPackets;
    void cleanupSeenPackets();
};

// Global mesh router instance
extern MeshRouter meshRouter;

#endif // MESH_ROUTING_H
