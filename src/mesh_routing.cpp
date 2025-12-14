/**
 * @file mesh_routing.cpp
 * @brief Mesh network routing implementation
 * @version 3.0.0
 * @date 2025-12-13
 */

#include "mesh_routing.h"
#include <Arduino.h>

// Global instance
MeshRouter meshRouter;

// Timeout for duplicate packet detection (30 seconds)
#define SEEN_PACKET_TIMEOUT_MS 30000

/**
 * @brief Constructor
 */
MeshRouter::MeshRouter() :
    nodeId(0),
    isBaseStation(false),
    forwardingEnabled(true),
    sequenceNumber(0),
    requestId(0),
    lastBeaconTime(0),
    lastRouteCleanup(0),
    lastNeighborCleanup(0),
    packetsForwarded(0),
    packetsDropped(0),
    routeDiscoveries(0)
{
}

/**
 * @brief Initialize mesh router
 */
void MeshRouter::begin(uint8_t id, bool isBase) {
    nodeId = id;
    isBaseStation = isBase;
    
    Serial.println("\n=== Mesh Router Initialized ===");
    Serial.printf("Node ID: %d\n", nodeId);
    Serial.printf("Mode: %s\n", isBaseStation ? "Base Station" : "Sensor Node");
    Serial.printf("Forwarding: %s\n", forwardingEnabled ? "Enabled" : "Disabled");
    
    // Base station doesn't need to discover routes (it's the destination)
    if (!isBaseStation) {
        // Start with base station route discovery
        Serial.println("Discovering route to base station (ID 1)...");
    }
}

/**
 * @brief Main loop - call periodically
 */
void MeshRouter::loop() {
    uint32_t now = millis();
    
    // Send periodic neighbor beacons
    if (now - lastBeaconTime >= NEIGHBOR_BEACON_INTERVAL_MS) {
        sendNeighborBeacon();
        lastBeaconTime = now;
    }
    
    // Cleanup expired routes (every 60 seconds)
    if (now - lastRouteCleanup >= 60000) {
        cleanupExpiredRoutes();
        lastRouteCleanup = now;
    }
    
    // Cleanup expired neighbors (every 60 seconds)
    if (now - lastNeighborCleanup >= 60000) {
        cleanupExpiredNeighbors();
        lastNeighborCleanup = now;
    }
    
    // Cleanup seen packets (every 30 seconds)
    static uint32_t lastSeenCleanup = 0;
    if (now - lastSeenCleanup >= 30000) {
        cleanupSeenPackets();
        lastSeenCleanup = now;
    }
}

/**
 * @brief Send a packet to destination
 */
bool MeshRouter::sendPacket(uint8_t destId, uint8_t* payload, size_t payloadSize) {
    // Check if we have a route
    RouteEntry* route = getRoute(destId);
    
    if (!route || !route->isValid) {
        // No route - try to discover one
        Serial.printf("No route to %d, discovering...\n", destId);
        if (!discoverRoute(destId)) {
            Serial.println("Route discovery failed");
            return false;
        }
        
        // Wait for route discovery
        uint32_t startTime = millis();
        while (millis() - startTime < ROUTE_DISCOVERY_TIMEOUT_MS) {
            route = getRoute(destId);
            if (route && route->isValid) {
                break;
            }
            delay(10);
        }
        
        if (!route || !route->isValid) {
            Serial.println("Route discovery timeout");
            return false;
        }
    }
    
    // Build mesh header
    MeshHeader header;
    header.packetType = MESH_DATA;
    header.sourceId = nodeId;
    header.destId = destId;
    header.nextHop = route->nextHop;
    header.prevHop = nodeId;
    header.hopCount = 0;
    header.ttl = MAX_HOPS;
    header.sequenceNum = getNextSequenceNumber();
    
    // TODO: Send packet via LoRa
    // For now, just log
    Serial.printf("Sending packet to %d via %d (seq %d)\n", 
                  destId, route->nextHop, header.sequenceNum);
    
    route->lastUsed = millis();
    return true;
}

/**
 * @brief Forward a received packet
 */
bool MeshRouter::forwardPacket(MeshHeader* header, uint8_t* payload, size_t payloadSize) {
    if (!forwardingEnabled) {
        return false;
    }
    
    // Check TTL
    if (header->ttl == 0) {
        Serial.println("Packet TTL expired, dropping");
        packetsDropped++;
        return false;
    }
    
    // Check if we're the destination
    if (header->destId == nodeId) {
        // Packet is for us, don't forward
        return false;
    }
    
    // Check for duplicates
    if (isPacketDuplicate(header->sourceId, header->sequenceNum)) {
        Serial.println("Duplicate packet, dropping");
        packetsDropped++;
        return false;
    }
    
    // Get route to destination
    RouteEntry* route = getRoute(header->destId);
    if (!route || !route->isValid) {
        Serial.printf("No route to forward packet to %d\n", header->destId);
        packetsDropped++;
        return false;
    }
    
    // Update header for forwarding
    header->prevHop = nodeId;
    header->nextHop = route->nextHop;
    header->hopCount++;
    header->ttl--;
    
    Serial.printf("Forwarding packet from %d to %d via %d (hop %d)\n",
                  header->sourceId, header->destId, route->nextHop, header->hopCount);
    
    // TODO: Send via LoRa
    packetsForwarded++;
    updateSeenPackets(header->sourceId, header->sequenceNum);
    route->lastUsed = millis();
    
    return true;
}

/**
 * @brief Process received packet
 */
void MeshRouter::processReceivedPacket(uint8_t* packet, size_t packetSize, int16_t rssi) {
    if (packetSize < sizeof(MeshHeader)) {
        return;
    }
    
    MeshHeader* header = (MeshHeader*)packet;
    
    // Handle different packet types
    switch (header->packetType) {
        case MESH_DATA:
            // Check if packet is for us or needs forwarding
            if (header->destId == nodeId || header->destId == 255) {
                // Packet is for us
                uint8_t* payload = packet + sizeof(MeshHeader);
                size_t payloadSize = packetSize - sizeof(MeshHeader);
                // TODO: Process data packet
            } else {
                // Forward packet
                uint8_t* payload = packet + sizeof(MeshHeader);
                size_t payloadSize = packetSize - sizeof(MeshHeader);
                forwardPacket(header, payload, payloadSize);
            }
            break;
            
        case MESH_ROUTE_REQUEST:
            processRouteRequest((RouteRequest*)packet, rssi);
            break;
            
        case MESH_ROUTE_REPLY:
            processRouteReply((RouteReply*)packet);
            break;
            
        case MESH_NEIGHBOR_BEACON:
            processNeighborBeacon((NeighborBeacon*)packet, rssi);
            break;
            
        default:
            Serial.printf("Unknown mesh packet type: %d\n", header->packetType);
            break;
    }
}

/**
 * @brief Check if route exists to destination
 */
bool MeshRouter::hasRouteTo(uint8_t destId) {
    RouteEntry* route = getRoute(destId);
    return (route != nullptr && route->isValid);
}

/**
 * @brief Get route to destination
 */
RouteEntry* MeshRouter::getRoute(uint8_t destId) {
    for (auto& route : routingTable) {
        if (route.destId == destId && route.isValid) {
            return &route;
        }
    }
    return nullptr;
}

/**
 * @brief Add new route
 */
void MeshRouter::addRoute(uint8_t destId, uint8_t nextHop, uint8_t hopCount) {
    // Check if route already exists
    RouteEntry* existing = getRoute(destId);
    if (existing) {
        // Update existing route if this one is better
        if (hopCount < existing->hopCount) {
            existing->nextHop = nextHop;
            existing->hopCount = hopCount;
            existing->lastUsed = millis();
            Serial.printf("Updated route to %d via %d (%d hops)\n", destId, nextHop, hopCount);
        }
        return;
    }
    
    // Add new route
    if (routingTable.size() >= MAX_ROUTES) {
        // Table full, remove oldest route
        uint32_t oldestTime = UINT32_MAX;
        size_t oldestIndex = 0;
        
        for (size_t i = 0; i < routingTable.size(); i++) {
            if (routingTable[i].lastUsed < oldestTime) {
                oldestTime = routingTable[i].lastUsed;
                oldestIndex = i;
            }
        }
        
        routingTable.erase(routingTable.begin() + oldestIndex);
        Serial.println("Routing table full, removed oldest entry");
    }
    
    RouteEntry newRoute;
    newRoute.destId = destId;
    newRoute.nextHop = nextHop;
    newRoute.hopCount = hopCount;
    newRoute.lastUsed = millis();
    newRoute.linkQuality = 0;
    newRoute.isValid = true;
    
    routingTable.push_back(newRoute);
    Serial.printf("Added route to %d via %d (%d hops)\n", destId, nextHop, hopCount);
}

/**
 * @brief Remove route
 */
void MeshRouter::removeRoute(uint8_t destId) {
    for (auto it = routingTable.begin(); it != routingTable.end(); ++it) {
        if (it->destId == destId) {
            routingTable.erase(it);
            Serial.printf("Removed route to %d\n", destId);
            return;
        }
    }
}

/**
 * @brief Update existing route
 */
void MeshRouter::updateRoute(uint8_t destId, uint8_t nextHop, uint8_t hopCount, int16_t rssi) {
    RouteEntry* route = getRoute(destId);
    if (route) {
        route->nextHop = nextHop;
        route->hopCount = hopCount;
        route->linkQuality = rssi;
        route->lastUsed = millis();
    } else {
        addRoute(destId, nextHop, hopCount);
    }
}

/**
 * @brief Cleanup expired routes
 */
void MeshRouter::cleanupExpiredRoutes() {
    uint32_t now = millis();
    size_t removed = 0;
    
    for (auto it = routingTable.begin(); it != routingTable.end(); ) {
        if (now - it->lastUsed > ROUTE_TIMEOUT_MS) {
            Serial.printf("Route to %d expired\n", it->destId);
            it = routingTable.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    
    if (removed > 0) {
        Serial.printf("Cleaned up %d expired routes\n", removed);
    }
}

/**
 * @brief Discover route to destination
 */
bool MeshRouter::discoverRoute(uint8_t destId) {
    sendRouteRequest(destId);
    routeDiscoveries++;
    return true;  // Async operation
}

/**
 * @brief Send route discovery request
 */
void MeshRouter::sendRouteRequest(uint8_t destId) {
    RouteRequest rreq;
    rreq.header.packetType = MESH_ROUTE_REQUEST;
    rreq.header.sourceId = nodeId;
    rreq.header.destId = 255;  // Broadcast
    rreq.header.nextHop = 255;
    rreq.header.prevHop = nodeId;
    rreq.header.hopCount = 0;
    rreq.header.ttl = MAX_HOPS;
    rreq.header.sequenceNum = getNextSequenceNumber();
    
    rreq.destId = destId;
    rreq.hopCount = 0;
    rreq.requestId = getNextRequestId();
    
    Serial.printf("Sending RREQ for %d (reqID %d)\n", destId, rreq.requestId);
    
    // TODO: Send via LoRa broadcast
}

/**
 * @brief Process route request
 */
void MeshRouter::processRouteRequest(RouteRequest* rreq, int16_t rssi) {
    Serial.printf("Received RREQ from %d for %d (hops %d)\n",
                  rreq->header.sourceId, rreq->destId, rreq->hopCount);
    
    // Check for duplicate
    if (isPacketDuplicate(rreq->header.sourceId, rreq->header.sequenceNum)) {
        Serial.println("Duplicate RREQ, ignoring");
        return;
    }
    updateSeenPackets(rreq->header.sourceId, rreq->header.sequenceNum);
    
    // Learn reverse route to source
    addRoute(rreq->header.sourceId, rreq->header.prevHop, rreq->hopCount + 1);
    
    // Are we the destination?
    if (rreq->destId == nodeId) {
        // Send route reply
        RouteReply rrep;
        rrep.header.packetType = MESH_ROUTE_REPLY;
        rrep.header.sourceId = nodeId;
        rrep.header.destId = rreq->header.sourceId;
        rrep.header.nextHop = rreq->header.prevHop;
        rrep.header.prevHop = nodeId;
        rrep.header.hopCount = 0;
        rrep.header.ttl = MAX_HOPS;
        rrep.header.sequenceNum = getNextSequenceNumber();
        
        rrep.destId = rreq->destId;
        rrep.hopCount = 0;
        rrep.requestId = rreq->requestId;
        
        Serial.printf("Sending RREP to %d\n", rreq->header.sourceId);
        // TODO: Send via LoRa
    } else if (forwardingEnabled && rreq->header.ttl > 0) {
        // Forward the request
        rreq->header.prevHop = nodeId;
        rreq->header.hopCount++;
        rreq->header.ttl--;
        rreq->hopCount++;
        
        Serial.println("Forwarding RREQ");
        // TODO: Send via LoRa broadcast
    }
}

/**
 * @brief Process route reply
 */
void MeshRouter::processRouteReply(RouteReply* rrep) {
    Serial.printf("Received RREP from %d for %d (hops %d)\n",
                  rrep->header.sourceId, rrep->destId, rrep->hopCount);
    
    // Learn route to destination
    addRoute(rrep->destId, rrep->header.prevHop, rrep->hopCount + 1);
    
    // Are we the original requester?
    if (rrep->header.destId == nodeId) {
        Serial.printf("Route discovery complete: %d hops to %d\n",
                      rrep->hopCount + 1, rrep->destId);
    } else if (forwardingEnabled && rrep->header.ttl > 0) {
        // Forward the reply
        RouteEntry* route = getRoute(rrep->header.destId);
        if (route) {
            rrep->header.nextHop = route->nextHop;
            rrep->header.prevHop = nodeId;
            rrep->header.hopCount++;
            rrep->header.ttl--;
            rrep->hopCount++;
            
            Serial.println("Forwarding RREP");
            // TODO: Send via LoRa
        }
    }
}

/**
 * @brief Send neighbor beacon
 */
void MeshRouter::sendNeighborBeacon() {
    NeighborBeacon beacon;
    beacon.header.packetType = MESH_NEIGHBOR_BEACON;
    beacon.header.sourceId = nodeId;
    beacon.header.destId = 255;  // Broadcast
    beacon.header.nextHop = 255;
    beacon.header.prevHop = nodeId;
    beacon.header.hopCount = 0;
    beacon.header.ttl = 1;  // Don't forward beacons
    beacon.header.sequenceNum = getNextSequenceNumber();
    
    beacon.neighborId = nodeId;
    beacon.rssi = 0;
    beacon.hopDistance = isBaseStation ? 0 : (hasRouteTo(1) ? getRoute(1)->hopCount : 255);
    
    Serial.printf("Sending neighbor beacon (hopDist %d)\n", beacon.hopDistance);
    // TODO: Send via LoRa broadcast
}

/**
 * @brief Process neighbor beacon
 */
void MeshRouter::processNeighborBeacon(NeighborBeacon* beacon, int16_t rssi) {
    uint8_t neighborId = beacon->neighborId;
    
    // Update or add neighbor
    Neighbor* neighbor = getNeighbor(neighborId);
    if (neighbor) {
        neighbor->rssi = rssi;
        neighbor->lastSeen = millis();
        neighbor->hopDistance = beacon->hopDistance;
        neighbor->isActive = true;
    } else {
        if (neighbors.size() >= MAX_ROUTES) {
            // Remove oldest inactive neighbor
            for (auto it = neighbors.begin(); it != neighbors.end(); ++it) {
                if (!it->isActive) {
                    neighbors.erase(it);
                    break;
                }
            }
        }
        
        Neighbor newNeighbor;
        newNeighbor.nodeId = neighborId;
        newNeighbor.rssi = rssi;
        newNeighbor.lastSeen = millis();
        newNeighbor.hopDistance = beacon->hopDistance;
        newNeighbor.isActive = true;
        
        neighbors.push_back(newNeighbor);
        Serial.printf("Discovered neighbor %d (RSSI %d, hopDist %d)\n",
                      neighborId, rssi, beacon->hopDistance);
        
        // If neighbor has route to base station, use it
        if (!isBaseStation && beacon->hopDistance < 255) {
            RouteEntry* baseRoute = getRoute(1);  // Base station ID = 1
            if (!baseRoute || baseRoute->hopCount > beacon->hopDistance + 1) {
                addRoute(1, neighborId, beacon->hopDistance + 1);
            }
        }
    }
}

/**
 * @brief Get neighbor info
 */
Neighbor* MeshRouter::getNeighbor(uint8_t nodeId) {
    for (auto& neighbor : neighbors) {
        if (neighbor.nodeId == nodeId) {
            return &neighbor;
        }
    }
    return nullptr;
}

/**
 * @brief Cleanup expired neighbors
 */
void MeshRouter::cleanupExpiredNeighbors() {
    uint32_t now = millis();
    size_t removed = 0;
    
    for (auto it = neighbors.begin(); it != neighbors.end(); ) {
        if (now - it->lastSeen > ROUTE_TIMEOUT_MS) {
            Serial.printf("Neighbor %d expired\n", it->nodeId);
            it = neighbors.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    
    if (removed > 0) {
        Serial.printf("Cleaned up %d expired neighbors\n", removed);
    }
}

/**
 * @brief Get network health (0-100%)
 */
float MeshRouter::getNetworkHealth() const {
    if (neighbors.empty()) {
        return 0.0f;
    }
    
    // Health based on active neighbors and route availability
    uint8_t activeNeighbors = 0;
    for (const auto& neighbor : neighbors) {
        if (neighbor.isActive) {
            activeNeighbors++;
        }
    }
    
    uint8_t validRoutes = 0;
    for (const auto& route : routingTable) {
        if (route.isValid) {
            validRoutes++;
        }
    }
    
    // Simple health metric: 50% neighbors + 50% routes
    float neighborHealth = (activeNeighbors * 100.0f) / MAX_ROUTES;
    float routeHealth = (validRoutes * 100.0f) / MAX_ROUTES;
    
    return (neighborHealth + routeHealth) / 2.0f;
}

/**
 * @brief Print routing table
 */
void MeshRouter::printRoutingTable() {
    Serial.println("\n=== Routing Table ===");
    Serial.printf("Routes: %d/%d\n", routingTable.size(), MAX_ROUTES);
    
    for (const auto& route : routingTable) {
        Serial.printf("  Dest %d -> NextHop %d (%d hops, RSSI %d, age %lus)\n",
                      route.destId, route.nextHop, route.hopCount,
                      route.linkQuality, (millis() - route.lastUsed) / 1000);
    }
    
    Serial.printf("Stats: Forwarded=%lu, Dropped=%lu, Discoveries=%lu\n",
                  packetsForwarded, packetsDropped, routeDiscoveries);
}

/**
 * @brief Print neighbor list
 */
void MeshRouter::printNeighbors() {
    Serial.println("\n=== Neighbors ===");
    Serial.printf("Count: %d\n", neighbors.size());
    
    for (const auto& neighbor : neighbors) {
        Serial.printf("  Node %d: RSSI %d, HopDist %d, Age %lus %s\n",
                      neighbor.nodeId, neighbor.rssi, neighbor.hopDistance,
                      (millis() - neighbor.lastSeen) / 1000,
                      neighbor.isActive ? "" : "[INACTIVE]");
    }
}

/**
 * @brief Get network topology as JSON
 */
String MeshRouter::getNetworkTopologyJSON() {
    String json = "{";
    json += "\"nodeId\":" + String(nodeId) + ",";
    json += "\"neighbors\":[";
    
    for (size_t i = 0; i < neighbors.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"id\":" + String(neighbors[i].nodeId);
        json += ",\"rssi\":" + String(neighbors[i].rssi);
        json += ",\"hopDist\":" + String(neighbors[i].hopDistance);
        json += ",\"active\":" + String(neighbors[i].isActive ? "true" : "false");
        json += "}";
    }
    
    json += "],\"routes\":[";
    
    for (size_t i = 0; i < routingTable.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"dest\":" + String(routingTable[i].destId);
        json += ",\"nextHop\":" + String(routingTable[i].nextHop);
        json += ",\"hops\":" + String(routingTable[i].hopCount);
        json += "}";
    }
    
    json += "]}";
    return json;
}

/**
 * @brief Check if packet is duplicate
 */
bool MeshRouter::isPacketDuplicate(uint8_t sourceId, uint16_t seqNum) {
    for (const auto& pkt : seenPackets) {
        if (pkt.sourceId == sourceId && pkt.seqNum == seqNum) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Add packet to seen list
 */
void MeshRouter::updateSeenPackets(uint8_t sourceId, uint16_t seqNum) {
    PacketId pkt;
    pkt.sourceId = sourceId;
    pkt.seqNum = seqNum;
    pkt.timestamp = millis();
    
    seenPackets.push_back(pkt);
    
    // Limit cache size
    if (seenPackets.size() > 100) {
        seenPackets.erase(seenPackets.begin());
    }
}

/**
 * @brief Cleanup old seen packets
 */
void MeshRouter::cleanupSeenPackets() {
    uint32_t now = millis();
    
    for (auto it = seenPackets.begin(); it != seenPackets.end(); ) {
        if (now - it->timestamp > SEEN_PACKET_TIMEOUT_MS) {
            it = seenPackets.erase(it);
        } else {
            ++it;
        }
    }
}

/**
 * @brief Get next sequence number
 */
uint16_t MeshRouter::getNextSequenceNumber() {
    return ++sequenceNumber;
}

/**
 * @brief Get next request ID
 */
uint16_t MeshRouter::getNextRequestId() {
    return ++requestId;
}

/**
 * @brief Get neighbor count
 */
uint8_t MeshRouter::getNeighborCount() const {
    uint8_t count = 0;
    for (const auto& neighbor : neighbors) {
        if (neighbor.isActive) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Get route count
 */
uint8_t MeshRouter::getRouteCount() const {
    uint8_t count = 0;
    for (const auto& route : routingTable) {
        if (route.isValid) {
            count++;
        }
    }
    return count;
}
