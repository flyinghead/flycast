/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

   Authors: Daniele Lacamera
 *********************************************************************/


#include "pico_protocol.h"
#include "pico_tree.h"

struct pico_proto_rr
{
    struct pico_tree *t;
    struct pico_tree_node *node_in, *node_out;
};


static int pico_proto_cmp(void *ka, void *kb)
{
    struct pico_protocol *a = ka, *b = kb;
    if (a->hash < b->hash)
        return -1;

    if (a->hash > b->hash)
        return 1;

    return 0;
}

static PICO_TREE_DECLARE(Datalink_proto_tree, pico_proto_cmp);
static PICO_TREE_DECLARE(Network_proto_tree, pico_proto_cmp);
static PICO_TREE_DECLARE(Transport_proto_tree, pico_proto_cmp);
static PICO_TREE_DECLARE(Socket_proto_tree, pico_proto_cmp);

/* Static variables to keep track of the round robin loop */
static struct pico_proto_rr proto_rr_datalink   = {
    &Datalink_proto_tree,     NULL, NULL
};
static struct pico_proto_rr proto_rr_network    = {
    &Network_proto_tree,      NULL, NULL
};
static struct pico_proto_rr proto_rr_transport  = {
    &Transport_proto_tree,    NULL, NULL
};
static struct pico_proto_rr proto_rr_socket     = {
    &Socket_proto_tree,       NULL, NULL
};

static int proto_loop_in(struct pico_protocol *proto, int loop_score)
{
    struct pico_frame *f;
    while(loop_score > 0) {
        if (proto->q_in->frames == 0)
            break;

        f = pico_dequeue(proto->q_in);
        if ((f) && (proto->process_in(proto, f) > 0)) {
            loop_score--;
        }
    }
    return loop_score;
}

static int proto_loop_out(struct pico_protocol *proto, int loop_score)
{
    struct pico_frame *f;
    while(loop_score > 0) {
        if (proto->q_out->frames == 0)
            break;

        f = pico_dequeue(proto->q_out);
        if ((f) && (proto->process_out(proto, f) > 0)) {
            loop_score--;
        }
    }
    return loop_score;
}

static int proto_loop(struct pico_protocol *proto, int loop_score, int direction)
{

    if (direction == PICO_LOOP_DIR_IN)
        loop_score = proto_loop_in(proto, loop_score);
    else if (direction == PICO_LOOP_DIR_OUT)
        loop_score = proto_loop_out(proto, loop_score);

    return loop_score;
}

static struct pico_tree_node *roundrobin_init(struct pico_proto_rr *rr, int direction)
{
    struct pico_tree_node *next_node = NULL;
    /* Initialization (takes place only once) */
    if (rr->node_in == NULL)
        rr->node_in = pico_tree_firstNode(rr->t->root);

    if (rr->node_out == NULL)
        rr->node_out = pico_tree_firstNode(rr->t->root);

    if (direction == PICO_LOOP_DIR_IN)
        next_node = rr->node_in;
    else
        next_node = rr->node_out;

    return next_node;
}

static void roundrobin_end(struct pico_proto_rr *rr, int direction, struct pico_tree_node *last)
{
    if (direction == PICO_LOOP_DIR_IN)
        rr->node_in = last;
    else
        rr->node_out = last;
}

static int pico_protocol_generic_loop(struct pico_proto_rr *rr, int loop_score, int direction)
{
    struct pico_protocol *start, *next;
    struct pico_tree_node *next_node = roundrobin_init(rr, direction);

    if (!next_node)
        return loop_score;

    next = next_node->keyValue;

    /* init start node */
    start = next;

    /* round-robin all layer protocols, break if traversed all protocols */
    while (loop_score > 1 && next != NULL) {
        loop_score = proto_loop(next, loop_score, direction);
        next_node = pico_tree_next(next_node);
        next = next_node->keyValue;
        if (next == NULL)
        {
            next_node = pico_tree_firstNode(rr->t->root);
            next = next_node->keyValue;
        }

        if (next == start)
            break;
    }
    roundrobin_end(rr, direction, next_node);
    return loop_score;
}

int pico_protocol_datalink_loop(int loop_score, int direction)
{
    return pico_protocol_generic_loop(&proto_rr_datalink, loop_score, direction);
}

int pico_protocol_network_loop(int loop_score, int direction)
{
    return pico_protocol_generic_loop(&proto_rr_network, loop_score, direction);
}

int pico_protocol_transport_loop(int loop_score, int direction)
{
    return pico_protocol_generic_loop(&proto_rr_transport, loop_score, direction);
}

int pico_protocol_socket_loop(int loop_score, int direction)
{
    return pico_protocol_generic_loop(&proto_rr_socket, loop_score, direction);
}

int pico_protocols_loop(int loop_score)
{
/*
   loop_score = pico_protocol_datalink_loop(loop_score);
   loop_score = pico_protocol_network_loop(loop_score);
   loop_score = pico_protocol_transport_loop(loop_score);
   loop_score = pico_protocol_socket_loop(loop_score);
 */
    return loop_score;
}

static void proto_layer_rr_reset(struct pico_proto_rr *rr)
{
    rr->node_in = NULL;
    rr->node_out = NULL;
}

void pico_protocol_init(struct pico_protocol *p)
{
    struct pico_tree *tree = NULL;
    struct pico_proto_rr *proto = NULL;

    if (!p)
        return;

    p->hash = pico_hash(p->name, (uint32_t)strlen(p->name));
    switch (p->layer) {
        case PICO_LAYER_DATALINK:
            tree = &Datalink_proto_tree;
            proto = &proto_rr_datalink;
            break;
        case PICO_LAYER_NETWORK:
            tree = &Network_proto_tree;
            proto = &proto_rr_network;
            break;
        case PICO_LAYER_TRANSPORT:
            tree = &Transport_proto_tree;
            proto = &proto_rr_transport;
            break;
        case PICO_LAYER_SOCKET:
            tree = &Socket_proto_tree;
            proto = &proto_rr_socket;
            break;
        default:
            dbg("Unknown protocol: %s (layer: %d)\n", p->name, p->layer);
            return;
    }

    if (pico_tree_insert(tree, p)) {
        dbg("Failed to insert protocol %s\n", p->name);
        return;
    }

    proto_layer_rr_reset(proto);
    dbg("Protocol %s registered (layer: %d).\n", p->name, p->layer);
}

