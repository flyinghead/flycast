/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   Author: Andrei Carp <andrei.carp@tass.be>
 *********************************************************************/

#include "pico_tree.h"
#include "pico_config.h"
#include "pico_protocol.h"
#include "pico_mm.h"

#define RED     0
#define BLACK 1

/* By default the null leafs are black */
struct pico_tree_node LEAF = {
    NULL, /* key */
    &LEAF, &LEAF, &LEAF, /* parent, left,right */
    BLACK, /* color */
};

#define IS_LEAF(x) (x == &LEAF)
#define IS_NOT_LEAF(x) (x != &LEAF)
#define INIT_LEAF (&LEAF)

#define AM_I_LEFT_CHILD(x) (x == x->parent->leftChild)
#define AM_I_RIGHT_CHILD(x) (x == x->parent->rightChild)

#define PARENT(x) (x->parent)
#define GRANPA(x) (x->parent->parent)

/*
 * Local Functions
 */
static struct pico_tree_node *create_node(struct pico_tree *tree, void *key, uint8_t allocator);
static void rotateToLeft(struct pico_tree*tree, struct pico_tree_node*node);
static void rotateToRight(struct pico_tree*root, struct pico_tree_node*node);
static void fix_insert_collisions(struct pico_tree*tree, struct pico_tree_node*node);
static void fix_delete_collisions(struct pico_tree*tree, struct pico_tree_node *node);
static void switchNodes(struct pico_tree*tree, struct pico_tree_node*nodeA, struct pico_tree_node*nodeB);
void *pico_tree_insert_implementation(struct pico_tree *tree, void *key, uint8_t allocator);
void *pico_tree_delete_implementation(struct pico_tree *tree, void *key, uint8_t allocator);

#ifdef PICO_SUPPORT_MM
/* The memory manager also uses the pico_tree to keep track of all the different slab sizes it has.
 * These nodes should be placed in the manager page which is in a different memory region then the nodes
 * which are used for the pico stack in general.
 * Therefore the following 2 functions are created so that pico_tree can use them to to put these nodes
 * into the correct memory regions.
 * If pico_tree_insert is called from the memory manager module, then create_node should use
 * pico_mem_page0_zalloc to create a node. The same for pico_tree_delete.
 */
extern void*pico_mem_page0_zalloc(size_t len);
extern void pico_mem_page0_free(void*ptr);
#endif  /* PICO_SUPPORT_MM */

/*
 * Exported functions
 */

struct pico_tree_node *pico_tree_firstNode(struct pico_tree_node *node)
{
    while(IS_NOT_LEAF(node->leftChild))
        node = node->leftChild;
    return node;
}

struct pico_tree_node *pico_tree_lastNode(struct pico_tree_node *node)
{
    while(IS_NOT_LEAF(node->rightChild))
        node = node->rightChild;
    return node;
}

struct pico_tree_node *pico_tree_next(struct pico_tree_node *node)
{
    if (!node)
        return NULL;

    if(IS_NOT_LEAF(node->rightChild))
    {
        node = node->rightChild;
        while(IS_NOT_LEAF(node->leftChild))
            node = node->leftChild;
    }
    else
    {
        if (IS_NOT_LEAF(node->parent) &&  AM_I_LEFT_CHILD(node))
            node = node->parent;
        else {
            while (IS_NOT_LEAF(node->parent) && AM_I_RIGHT_CHILD(node))
                node = node->parent;
            node = node->parent;
        }
    }

    return node;
}

struct pico_tree_node *pico_tree_prev(struct pico_tree_node *node)
{
    if (IS_NOT_LEAF(node->leftChild)) {
        node = node->leftChild;
        while (IS_NOT_LEAF(node->rightChild))
            node = node->rightChild;
    } else {
        if (IS_NOT_LEAF(node->parent) && AM_I_RIGHT_CHILD(node))
            node = node->parent;
        else {
            while (IS_NOT_LEAF(node) && AM_I_LEFT_CHILD(node))
                node = node->parent;
            node = node->parent;
        }
    }

    return node;
}

/* The memory manager also uses the pico_tree to keep track of all the different slab sizes it has.
 * These nodes should be placed in the manager page which is in a different memory region then the nodes
 * which are used for the pico stack in general.
 * Therefore the following wrapper for pico_tree_insert is created.
 * The actual implementation can be found in pico_tree_insert_implementation.
 */
void *pico_tree_insert(struct pico_tree *tree, void *key)
{
    return pico_tree_insert_implementation(tree, key, USE_PICO_ZALLOC);
}

static void pico_tree_insert_node(struct pico_tree *tree, struct pico_tree_node *insert)
{
    struct pico_tree_node *temp = tree->root;
    struct pico_tree_node *last_node = INIT_LEAF;
    int result = 0;

    /* search for the place to insert the new node */
    while(IS_NOT_LEAF(temp))
    {
        last_node = temp;
        result = tree->compare(insert->keyValue, temp->keyValue);

        temp = (result < 0) ? (temp->leftChild) : (temp->rightChild);
    }
    /* make the needed connections */
    insert->parent = last_node;

    if(IS_LEAF(last_node))
        tree->root = insert;
    else{
        result = tree->compare(insert->keyValue, last_node->keyValue);
        if(result < 0)
            last_node->leftChild = insert;
        else
            last_node->rightChild = insert;
    }
}

void *pico_tree_insert_implementation(struct pico_tree *tree, void *key, uint8_t allocator)
{
    struct pico_tree_node *insert;
    void *LocalKey;

    LocalKey = (IS_NOT_LEAF(tree->root) ? pico_tree_findKey(tree, key) : NULL);

    /* if node already in, bail out */
    if(LocalKey) {
        pico_err = PICO_ERR_EEXIST;
        return LocalKey;
    }

    insert = create_node(tree, key, allocator);

    if(!insert)
    {
        pico_err = PICO_ERR_ENOMEM;
        /* to let the user know that it couldn't insert */
        return (void *)&LEAF;
    }

    pico_tree_insert_node(tree, insert);

    /* fix colour issues */
    fix_insert_collisions(tree, insert);

    return NULL;
}

struct pico_tree_node *pico_tree_findNode(struct pico_tree *tree, void *key)
{
    struct pico_tree_node *found;

    found = tree->root;

    while(IS_NOT_LEAF(found))
    {
        int result;
        result = tree->compare(found->keyValue, key);
        if(result == 0)
            return found;
        else if(result < 0)
            found = found->rightChild;
        else
            found = found->leftChild;
    }
    return NULL;
}

void *pico_tree_findKey(struct pico_tree *tree, void *key)
{
    struct pico_tree_node *found;

    found = pico_tree_findNode(tree, key);
    if (found == NULL)
        return NULL;
    return found->keyValue;
}

void *pico_tree_first(struct pico_tree *tree)
{
    return pico_tree_firstNode(tree->root)->keyValue;
}

void *pico_tree_last(struct pico_tree *tree)
{
    return pico_tree_lastNode(tree->root)->keyValue;
}

static uint8_t pico_tree_delete_node(struct pico_tree *tree, struct pico_tree_node *d, struct pico_tree_node **temp)
{
    struct pico_tree_node *min;
    struct pico_tree_node *ltemp = d;
    uint8_t nodeColor;
    min = pico_tree_firstNode(d->rightChild);
    nodeColor = min->color;

    *temp = min->rightChild;
    if(min->parent == ltemp && IS_NOT_LEAF(*temp))
        (*temp)->parent = min;
    else{
        switchNodes(tree, min, min->rightChild);
        min->rightChild = ltemp->rightChild;
        if(IS_NOT_LEAF(min->rightChild)) min->rightChild->parent = min;
    }

    switchNodes(tree, ltemp, min);
    min->leftChild = ltemp->leftChild;

    if(IS_NOT_LEAF(min->leftChild))
        min->leftChild->parent = min;

    min->color = ltemp->color;
    return nodeColor;
}

static uint8_t pico_tree_delete_check_switch(struct pico_tree *tree, struct pico_tree_node *delete, struct pico_tree_node **temp)
{
    struct pico_tree_node *ltemp = delete;
    uint8_t nodeColor = delete->color;
    if(IS_LEAF(delete->leftChild))
    {
        *temp = ltemp->rightChild;
        switchNodes(tree, ltemp, ltemp->rightChild);
    }
    else
    if(IS_LEAF(delete->rightChild))
    {
        struct pico_tree_node *_ltemp = delete;
        *temp = _ltemp->leftChild;
        switchNodes(tree, _ltemp, _ltemp->leftChild);
    }
    else{
        nodeColor = pico_tree_delete_node(tree, delete, temp);
    }

    return nodeColor;

}

/* The memory manager also uses the pico_tree to keep track of all the different slab sizes it has.
 * These nodes should be placed in the manager page which is in a different memory region then the nodes
 * which are used for the pico stack in general.
 * Therefore the following wrapper for pico_tree_delete is created.
 * The actual implementation can be found in pico_tree_delete_implementation.
 */
void *pico_tree_delete(struct pico_tree *tree, void *key)
{
    return pico_tree_delete_implementation(tree, key, USE_PICO_ZALLOC);
}

static inline void if_nodecolor_black_fix_collisions(struct pico_tree *tree, struct pico_tree_node *temp, uint8_t nodeColor)
{
    /* deleted node is black, this will mess up the black path property */
    if(nodeColor == BLACK)
        fix_delete_collisions(tree, temp);
}

void *pico_tree_delete_implementation(struct pico_tree *tree, void *key, uint8_t allocator)
{
    struct pico_tree_node *temp;
    uint8_t nodeColor; /* keeps the color of the node to be deleted */
    void *lkey; /* keeps a copy of the key which will be removed */
    struct pico_tree_node *delete;  /* keeps a copy of the node to be extracted */
    if (!key)
        return NULL;

    delete = pico_tree_findNode(tree, key);

    /* this key isn't in the tree, bail out */
    if(!delete)
        return NULL;

    lkey = delete->keyValue;
    nodeColor = pico_tree_delete_check_switch(tree, delete, &temp);

    if_nodecolor_black_fix_collisions(tree, temp, nodeColor);

    if(allocator == USE_PICO_ZALLOC)
        PICO_FREE(delete);

#ifdef PICO_SUPPORT_MM
    else
        pico_mem_page0_free(delete);
#endif
    return lkey;
}

int pico_tree_empty(struct pico_tree *tree)
{
    return (!tree->root || IS_LEAF(tree->root));
}

/*
 * Private functions
 */
static void rotateToLeft(struct pico_tree*tree, struct pico_tree_node*node)
{
    struct pico_tree_node*temp;

    temp = node->rightChild;

    if(temp == &LEAF) return;

    node->rightChild = temp->leftChild;

    if(IS_NOT_LEAF(temp->leftChild))
        temp->leftChild->parent = node;

    temp->parent = node->parent;

    if(IS_LEAF(node->parent))
        tree->root = temp;
    else
    if(node == node->parent->leftChild)
        node->parent->leftChild = temp;
    else
        node->parent->rightChild = temp;

    temp->leftChild = node;
    node->parent = temp;
}


static void rotateToRight(struct pico_tree *tree, struct pico_tree_node *node)
{
    struct pico_tree_node*temp;

    temp = node->leftChild;
    node->leftChild = temp->rightChild;

    if(temp == &LEAF) return;

    if(IS_NOT_LEAF(temp->rightChild))
        temp->rightChild->parent = node;

    temp->parent = node->parent;

    if(IS_LEAF(node->parent))
        tree->root = temp;
    else
    if(node == node->parent->rightChild)
        node->parent->rightChild = temp;
    else
        node->parent->leftChild = temp;

    temp->rightChild = node;
    node->parent = temp;
    return;
}

static struct pico_tree_node *create_node(struct pico_tree *tree, void*key, uint8_t allocator)
{
    struct pico_tree_node *temp = NULL;
    IGNORE_PARAMETER(tree);
    if(allocator == USE_PICO_ZALLOC)
        temp = (struct pico_tree_node *)PICO_ZALLOC(sizeof(struct pico_tree_node));

#ifdef PICO_SUPPORT_MM
    else
        temp = (struct pico_tree_node *)pico_mem_page0_zalloc(sizeof(struct pico_tree_node));
#endif

    if(!temp)
        return NULL;

    temp->keyValue = key;
    temp->parent = &LEAF;
    temp->leftChild = &LEAF;
    temp->rightChild = &LEAF;
    /* by default every new node is red */
    temp->color = RED;
    return temp;
}

/*
 * This function fixes the possible collisions in the tree.
 * Eg. if a node is red his children must be black !
 */
static void fix_insert_collisions(struct pico_tree*tree, struct pico_tree_node*node)
{
    struct pico_tree_node*temp;

    while(node->parent->color == RED && IS_NOT_LEAF(GRANPA(node)))
    {
        if(AM_I_RIGHT_CHILD(node->parent))
        {
            temp = GRANPA(node)->leftChild;
            if(temp->color == RED) {
                node->parent->color = BLACK;
                temp->color = BLACK;
                GRANPA(node)->color = RED;
                node = GRANPA(node);
            }
            else if(temp->color == BLACK) {
                if(AM_I_LEFT_CHILD(node)) {
                    node = node->parent;
                    rotateToRight(tree, node);
                }

                node->parent->color = BLACK;
                GRANPA(node)->color = RED;
                rotateToLeft(tree, GRANPA(node));
            }
        }
        else if(AM_I_LEFT_CHILD(node->parent))
        {
            temp = GRANPA(node)->rightChild;
            if(temp->color == RED) {
                node->parent->color = BLACK;
                temp->color = BLACK;
                GRANPA(node)->color = RED;
                node = GRANPA(node);
            }
            else if(temp->color == BLACK) {
                if(AM_I_RIGHT_CHILD(node)) {
                    node = node->parent;
                    rotateToLeft(tree, node);
                }

                node->parent->color = BLACK;
                GRANPA(node)->color = RED;
                rotateToRight(tree, GRANPA(node));
            }
        }
    }
    /* make sure that the root of the tree stays black */
    tree->root->color = BLACK;
}

static void switchNodes(struct pico_tree*tree, struct pico_tree_node*nodeA, struct pico_tree_node*nodeB)
{

    if(IS_LEAF(nodeA->parent))
        tree->root = nodeB;
    else
    if(IS_NOT_LEAF(nodeA))
    {
        if(AM_I_LEFT_CHILD(nodeA))
            nodeA->parent->leftChild = nodeB;
        else
            nodeA->parent->rightChild = nodeB;
    }

    if(IS_NOT_LEAF(nodeB)) nodeB->parent = nodeA->parent;

}

/*
 * This function fixes the possible collisions in the tree.
 * Eg. if a node is red his children must be black !
 * In this case the function fixes the constant black path property.
 */
static void fix_delete_collisions(struct pico_tree*tree, struct pico_tree_node *node)
{
    struct pico_tree_node*temp;

    while( node != tree->root && node->color == BLACK && IS_NOT_LEAF(node))
    {
        if(AM_I_LEFT_CHILD(node)) {

            temp = node->parent->rightChild;
            if(temp->color == RED)
            {
                temp->color = BLACK;
                node->parent->color = RED;
                rotateToLeft(tree, node->parent);
                temp = node->parent->rightChild;
            }

            if(temp->leftChild->color == BLACK && temp->rightChild->color == BLACK)
            {
                temp->color = RED;
                node = node->parent;
            }
            else
            {
                if(temp->rightChild->color == BLACK)
                {
                    temp->leftChild->color = BLACK;
                    temp->color = RED;
                    rotateToRight(tree, temp);
                    temp = temp->parent->rightChild;
                }

                temp->color = node->parent->color;
                node->parent->color = BLACK;
                temp->rightChild->color = BLACK;
                rotateToLeft(tree, node->parent);
                node = tree->root;
            }
        }
        else{
            temp = node->parent->leftChild;
            if(temp->color == RED)
            {
                temp->color = BLACK;
                node->parent->color = RED;
                rotateToRight(tree, node->parent);
                temp = node->parent->leftChild;
            }

            if(temp->rightChild->color == BLACK && temp->leftChild->color == BLACK)
            {
                temp->color = RED;
                node = node->parent;
            }
            else{
                if(temp->leftChild->color == BLACK)
                {
                    temp->rightChild->color = BLACK;
                    temp->color = RED;
                    rotateToLeft(tree, temp);
                    temp = temp->parent->leftChild;
                }

                temp->color = node->parent->color;
                node->parent->color = BLACK;
                temp->leftChild->color = BLACK;
                rotateToRight(tree, node->parent);
                node = tree->root;
            }
        }
    }
    node->color = BLACK;
}
