#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hash_table.h"
#include "neighbour.h"
#include "packet.h"
#include "requests.h"
#include "server.h"
#include "util.h"

// actual underlying hash table
htable **ht = NULL;
rtable **rt = NULL;

// chord peers
peer *self = NULL;
peer *pred = NULL;
peer *succ = NULL;

/**
 * @brief Forward a packet to a peer.
 *
 * @param peer The peer to forward the request to
 * @param pack The packet to forward
 * @return int The status of the sending procedure
 */
int forward(peer *p, packet *pack) {
    /* TODO IMPLEMENT */
    if (peer_connect(p) == 0) {
    	size_t len;
	unsigned char *p_s = packet_serialize(pack, &len);
	int stat = sendall(p->socket, p_s, len);
	peer_disconnect(p);
	return stat;
    }
    return -1;
}

/**
 * @brief Forward a request to the successor.
 *
 * @param srv The server
 * @param csocket The scokent of the client
 * @param p The packet to forward
 * @param n The peer to forward to
 * @return int The callback status
 */
int proxy_request(server *srv, int csocket, packet *p, peer *n) {
    /* TODO IMPLEMENT */
    
    if (peer_connect(n) == 0) {

        // send request
        size_t len;
        unsigned char *p_s = packet_serialize(p, &len);
        sendall(n->socket, p_s, len);


        // get response
        size_t response_len = 0;
        unsigned char *response = recvall(n->socket, &response_len);

        sendall(csocket, response, response_len);

        free(response);
        return -1;
    }

    return CB_REMOVE_CLIENT;
}

/**
 * @brief Lookup the peer responsible for a hash_id.
 *
 * @param hash_id The hash to lookup
 * @return int The callback status
 */
int lookup_peer(uint16_t hash_id) {
    /* TODO IMPLEMENT */

    // create packer
    packet *lpeer = packet_new();
    lpeer->flags = PKT_FLAG_CTRL | PKT_FLAG_LKUP;
    lpeer->hash_id = hash_id;
    lpeer->node_id = self->node_id;
    lpeer->node_port = self->port;

    lpeer->node_ip = peer_get_ip(self);

    // send
    forward(succ, lpeer);
    
    return 0;
}

/**
 * @brief Handle a client request we are resonspible for.
 *
 * @param srv The server
 * @param c The client
 * @param p The packet
 * @return int The callback status
 */
int handle_own_request(server *srv, client *c, packet *p) {
    /* TODO IMPLEMENT */

    packet *response = packet_new();

    // Get Request
    if (p->flags & PKT_FLAG_GET) {

        htable *exist = htable_get(ht, p->key, p->key_len);
        if (exist != NULL) {
            response->flags = PKT_FLAG_GET | PKT_FLAG_ACK;

            response->key_len = exist->key_len;
            response->key = (unsigned char *)malloc(exist->key_len);
            memcpy(response->key, exist->key, exist->key_len);

            response->value_len = exist->value_len;
            response->value = (unsigned char *)malloc(exist->value_len);
            memcpy(response->value, exist->value, exist->value_len);

        }
        else {
            // not in hash table
            response->flags = PKT_FLAG_GET;
            response->key_len = p->key_len;
            response->key = (unsigned char *)malloc(p->key_len);
            memcpy(response->key, p->key, p->key_len);
        }

    }

    // Set Request
    else if (p->flags & PKT_FLAG_SET) {
        response->flags = PKT_FLAG_SET | PKT_FLAG_ACK;
        htable_set(ht, p->key, p->key_len, p->value, p->value_len);

    }

    // Delete Request
    else if (p->flags & PKT_FLAG_DEL) {
        int stat = htable_delete(ht, p->key, p->key_len);

        if (stat == 0) {
            response->flags = PKT_FLAG_DEL | PKT_FLAG_ACK;
        }
        else {
            response->flags = PKT_FLAG_DEL;
        }
    }

    // send the response
    size_t len;
    unsigned char *mail = packet_serialize(response, &len);
    sendall(c->socket, mail, len);

    free (response);


    return CB_REMOVE_CLIENT;
}

/**
 * @brief Answer a lookup request from a peer.
 *
 * @param p The packet
 * @param n The peer
 * @return int The callback status
 */
int answer_lookup(packet *p, peer *n) {
    /* TODO IMPLEMENT */
    peer *asker = peer_from_packet(p);

    if (peer_connect(asker) != 0) {
        return CB_REMOVE_CLIENT;
    }

    // creating a packet
    packet *response = packet_new();
    response->flags = PKT_FLAG_CTRL | PKT_FLAG_RPLY;
    response->hash_id = p->hash_id;
    response->node_id = n->node_id;
    response->node_port = n->port;
    response->node_ip = peer_get_ip(n);

    // send
    size_t len;
    unsigned char *mail = packet_serialize(response, &len);
    sendall(asker->socket, mail, len);

    // free
    free(response);
    peer_disconnect(asker);
    peer_free(asker);
    return CB_REMOVE_CLIENT;
}

/**
 * @brief Handle a key request request from a client.
 *
 * @param srv The server
 * @param c The client
 * @param p The packet
 * @return int The callback status
 */
int handle_packet_data(server *srv, client *c, packet *p) {
    // Hash the key of the <key, value> pair to use for the hash table
    uint16_t hash_id = pseudo_hash(p->key, p->key_len);
    fprintf(stderr, "Hash id: %d\n", hash_id);

    // Forward the packet to the correct peer
    if (peer_is_responsible(pred->node_id, self->node_id, hash_id)) {
        // We are responsible for this key
        fprintf(stderr, "We are responsible.\n");
        return handle_own_request(srv, c, p);
    } else if (peer_is_responsible(self->node_id, succ->node_id, hash_id)) {
        // Our successor is responsible for this key
        fprintf(stderr, "Successor's business.\n");
        return proxy_request(srv, c->socket, p, succ);
    } else {
        // We need to find the peer responsible for this key
        fprintf(stderr, "No idea! Just looking it up!.\n");
        add_request(rt, hash_id, c->socket, p);
        lookup_peer(hash_id);
        return CB_OK;
    }
}

/**
 * @brief Handle a control packet from another peer.
 * Lookup vs. Proxy Reply
 *
 * @param srv The server
 * @param c The client
 * @param p The packet
 * @return int The callback status
 */
int handle_packet_ctrl(server *srv, client *c, packet *p) {

    fprintf(stderr, "Handling control packet...\n");

    if (p->flags & PKT_FLAG_LKUP) {
        // we received a lookup request
        if (peer_is_responsible(pred->node_id, self->node_id, p->hash_id)) {
            // Our business
            fprintf(stderr, "Lol! This should not happen!\n");
            return answer_lookup(p, self);
        } else if (peer_is_responsible(self->node_id, succ->node_id,
                                       p->hash_id)) {
            return answer_lookup(p, succ);
        } else {
            // Great! Somebody else's job!
            forward(succ, p);
        }
    } else if (p->flags & PKT_FLAG_RPLY) {
        // Look for open requests and proxy them
        peer *n = peer_from_packet(p);
        for (request *r = get_requests(rt, p->hash_id); r != NULL;
             r = r->next) {
            proxy_request(srv, r->socket, r->packet, n);
            server_close_socket(srv, r->socket);
        }
        clear_requests(rt, p->hash_id);
    } else {
    }
    return CB_REMOVE_CLIENT;
}

/**
 * @brief Handle a received packet.
 * This can be a key request received from a client or a control packet from
 * another peer.
 *
 * @param srv The server instance
 * @param c The client instance
 * @param p The packet instance
 * @return int The callback status
 */
int handle_packet(server *srv, client *c, packet *p) {
    if (p->flags & PKT_FLAG_CTRL) {
        return handle_packet_ctrl(srv, c, p);
    } else {
        return handle_packet_data(srv, c, p);
    }
}

/**
 * @brief Main entry for a peer of the chord ring.
 *
 * Requires 9 arguments:
 * 1. Id
 * 2. Hostname
 * 3. Port
 * 4. Id of the predecessor
 * 5. Hostname of the predecessor
 * 6. Port of the predecessor
 * 7. Id of the successor
 * 8. Hostname of the successor
 * 9. Port of the successor
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @return int The exit code
 */
int main(int argc, char **argv) {

    if (argc < 10) {
        fprintf(stderr, "Not enough args! I need ID IP PORT ID_P IP_P PORT_P "
                        "ID_S IP_S PORT_S\n");
    }

    // Read arguments for self
    uint16_t idSelf = strtoul(argv[1], NULL, 10);
    char *hostSelf = argv[2];
    char *portSelf = argv[3];

    // Read arguments for predecessor
    uint16_t idPred = strtoul(argv[4], NULL, 10);
    char *hostPred = argv[5];
    char *portPred = argv[6];

    // Read arguments for successor
    uint16_t idSucc = strtoul(argv[7], NULL, 10);
    char *hostSucc = argv[8];
    char *portSucc = argv[9];

    // Initialize all chord peers
    self = peer_init(
        idSelf, hostSelf,
        portSelf); //  Not really necessary but convenient to store us as a peer
    pred = peer_init(idPred, hostPred, portPred); //

    succ = peer_init(idSucc, hostSucc, portSucc);

    // Initialize outer server for communication with clients
    server *srv = server_setup(portSelf);
    if (srv == NULL) {
        fprintf(stderr, "Server setup failed!\n");
        return -1;
    }
    // Initialize hash table
    ht = (htable **)malloc(sizeof(htable *));
    // Initiale reuqest table
    rt = (rtable **)malloc(sizeof(rtable *));
    *ht = NULL;
    *rt = NULL;

    srv->packet_cb = handle_packet;
    server_run(srv);
    close(srv->socket);
}
