/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#ifndef AVL_H_
#define AVL_H_

// AVL tree implementation

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "washdc/error.h"

typedef uint32_t avl_key_type;

#define AVL_DEREF(nodep, tp, memb)                      \
    (*((tp*)(((uint8_t*)nodep) - offsetof(tp, memb))))

struct avl_node {
    avl_key_type key;
    int bal;
    struct avl_node *left, *right, *parent;
};

struct avl_node;

typedef struct avl_node*(*avl_node_ctor)(void);
typedef void(*avl_node_dtor)(struct avl_node*);

struct avl_tree {
    struct avl_node *root;

    /*
     * TODO: instead of storing ctor and dtor in the struct, all of the
     * functions in this header should be implemented at compiletime using
     * macros so that the ctor and dtor functions can be hardcoded.
     */
    avl_node_ctor ctor;
    avl_node_dtor dtor;
};

static inline void
avl_init(struct avl_tree *tree, avl_node_ctor ctor, avl_node_dtor dtor) {
    memset(tree, 0, sizeof(*tree));
    tree->ctor = ctor;
    tree->dtor = dtor;
}

static inline void
avl_clear_node(struct avl_tree *tree, struct avl_node *node) {
    if (node) {
        if (node->left)
            avl_clear_node(tree, node->left);
        if (node->right)
            avl_clear_node(tree, node->right);
        tree->dtor(node);
    }
}

static inline void
avl_cleanup(struct avl_tree *tree) {
    avl_clear_node(tree, tree->root);
    memset(tree, 0, sizeof(*tree));
}

#ifdef INVARIANTS
static int avl_height(struct avl_node *node) {
    int max_height = 0;
    if (node->left) {
        int left_height = avl_height(node->left) + 1;
        if (left_height > max_height)
            max_height = left_height;
    }
    if (node->right) {
        int right_height = avl_height(node->right) + 1;
        if (right_height > max_height)
            max_height = right_height;
    }
    return max_height;
}

static int avl_balance(struct avl_node *node) {
    int left_height = 0, right_height = 0;

    if (node->right)
        right_height = 1 + avl_height(node->right);
    if (node->left)
        left_height = 1 + avl_height(node->left);

    int bal = right_height - left_height;

    return bal;
}

static void avl_invariant(struct avl_node *node) {
    int bal = avl_balance(node);
    if (abs(bal) > 1) {
        LOG_ERROR("node balance is %d\n", bal);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    if (node->left)
        avl_invariant(node->left);
    if (node->right)
        avl_invariant(node->right);
}
#endif


/*
 * rotate the subtree right-wards so that the left child is now the root-node.
 * The original root-node will become the right node.
 *
 * The onus is on the caller to make sure the left child exists before calling
 * this function.
 *
 * This function DOES NOT update the balance factors; it is entirely on the
 * caller to do that.
 */
static void avl_rot_right(struct avl_tree *tree, struct avl_node *old_root) {
    struct avl_node *parent = old_root->parent;
    struct avl_node *new_root = old_root->left;
    struct avl_node *new_left_subtree = new_root->right;

    if (old_root != tree->root && !parent)
        RAISE_ERROR(ERROR_INTEGRITY);

    // update the parent's view of this subtree
    if (parent) {
        if (parent->left == old_root)
            parent->left = new_root;
        else
            parent->right = new_root;
    }

    new_root->parent = parent;
    old_root->parent = new_root;
    if (new_left_subtree)
        new_left_subtree->parent = old_root;

    old_root->left = new_left_subtree;
    new_root->right = old_root;

    if (tree->root == old_root)
        tree->root = new_root;
}

/*
 * rotate the subtree left-wards so that the right child is now the root-node.
 * The original root-node will become the left node.
 *
 * The onus is on the caller to make sure the right child exists before calling
 * this function.
 *
 * This function DOES NOT update the balance factors; it is entirely on the
 * caller to do that.
 */
static void avl_rot_left(struct avl_tree *tree, struct avl_node *old_root) {
    struct avl_node *parent = old_root->parent;
    struct avl_node *new_root = old_root->right;
    struct avl_node *new_right_subtree = new_root->left;

    if (old_root != tree->root && !parent)
        RAISE_ERROR(ERROR_INTEGRITY);

    // update the parent's view of this subtree
    if (parent) {
        if (parent->left == old_root)
            parent->left = new_root;
        else
            parent->right = new_root;
    }

    new_root->parent = parent;
    old_root->parent = new_root;
    if (new_right_subtree)
        new_right_subtree->parent = old_root;

    old_root->right = new_right_subtree;
    new_root->left = old_root;

    if (tree->root == old_root)
        tree->root = new_root;
}

static inline struct avl_node *
avl_basic_insert(struct avl_tree *tree, struct avl_node **node_p,
                 struct avl_node *parent, avl_key_type key) {
    struct avl_node *new_node = tree->ctor();
    if (!new_node)
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    *node_p = new_node;
    if (node_p != &tree->root && !parent)
        RAISE_ERROR(ERROR_INTEGRITY);
    new_node->parent = parent;
    new_node->key = key;

    /*
     * now retrace back up to the root using a the AVL rebalancing algorithm
     * to ensure that the heights of each node's subtrees differ by no more
     * than 1.
     */
    struct avl_node *cur_node = new_node;
    while (cur_node != tree->root) {
        struct avl_node *parent = cur_node->parent;
        if (cur_node == parent->left) {
            switch (parent->bal) {
            case 1:
                // parent-node height is unchanged
                parent->bal = 0;
                goto the_end;
            case 0:
                /*
                 * the parent-node does not need to be rebalanced, but its
                 * height has changed.
                 */
                parent->bal = -1;
                break;
            case -1:
                /*
                 * the parent-node is completely imbalanced and needs to be
                 * rotated.
                 */
                if (cur_node->bal <= 0) {
                    avl_rot_right(tree, parent);
                    parent->bal = 0;
                    cur_node->bal = 0;
                } else {
                    int child_bal = cur_node->right->bal;
                    avl_rot_left(tree, cur_node);
                    avl_rot_right(tree, parent);
                    if (child_bal < 0) {
                        cur_node->bal = 0;
                        parent->bal = 1;
                    } else if (child_bal > 0) {
                        cur_node->bal = -1;
                        parent->bal = 0;
                    } else {
                        cur_node->bal = 0;
                        parent->bal = 0;
                    }
                    cur_node->parent->bal = 0;
                }
                goto the_end;
            default:
                // should be impossible
                RAISE_ERROR(ERROR_INTEGRITY);
            }
        } else {
            switch (parent->bal) {
            case -1:
                // parent-node height is unchanged
                parent->bal = 0;
                goto the_end;
            case 0:
                /*
                 * the parent-node does not need to be rebalanced, but its
                 * height has changed.
                 */
                parent->bal = 1;
                break;
            case 1:
                /*
                 * the parent-node is completely imbalanced and needs to be
                 * rotated.
                 */
                if (cur_node->bal >= 0) {
                    avl_rot_left(tree, parent);
                    parent->bal = 0;
                    cur_node->bal = 0;
                } else {
                    int child_bal = cur_node->left->bal;
                    avl_rot_right(tree, cur_node);
                    avl_rot_left(tree, parent);
                    if (child_bal < 0) {
                        parent->bal = 0;
                        cur_node->bal = 1;
                    } else if (child_bal > 0) {
                        cur_node->bal = 0;
                        parent->bal = -1;
                    } else {
                        cur_node->bal = 0;
                        parent->bal = 0;
                    }
                    cur_node->parent->bal = 0;
                }
                goto the_end;
            default:
                // should be impossible
                RAISE_ERROR(ERROR_INTEGRITY);
            }
        }
        cur_node = parent;
    }

 the_end:
#ifdef INVARIANTS
    avl_invariant(tree->root);
#endif

    return new_node;
}

static inline struct avl_node*
avl_find(struct avl_tree *tree, avl_key_type key) {
    struct avl_node *node = tree->root;
    if (node) {
        for (;;) {
            if (key < node->key) {
                if (node->left) {
                    node = node->left;
                    continue;
                }
                return avl_basic_insert(tree, &node->left, node, key);
            }

            if (key > node->key) {
                if (node->right) {
                    node = node->right;
                    continue;
                }
                return avl_basic_insert(tree, &node->right, node, key);
            }

            return node;
        }
    } else {
        // empty tree, insert at root node
        return avl_basic_insert(tree, &tree->root, NULL, key);
    }
}

/*
 * This is like avl_find except if it fails it will return NULL instead of
 * creating a new node.
 */
static inline struct avl_node*
avl_find_noinsert(struct avl_tree *tree, avl_key_type key) {
    struct avl_node *node = tree->root;
    if (node) {
        for (;;) {
            if (key < node->key) {
                if (node->left) {
                    node = node->left;
                    continue;
                }
                return NULL;
            }

            if (key > node->key) {
                if (node->right) {
                    node = node->right;
                    continue;
                }
                return NULL;
            }

            return node;
        }
    } else {
        // empty tree
        return NULL;
    }
}

#endif
