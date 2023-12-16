/*
 * Compact Binary Trees - exported functions for operations on string keys
 *
 * Copyright (C) 2014-2023 Willy Tarreau - w@1wt.eu
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*
 * These trees are optimized for adding the minimalest overhead to the stored
 * data. This version uses the node's pointer as the key, for the purpose of
 * quickly finding its neighbours.
 *
 * A few properties :
 * - the xor between two branches of a node cannot be zero since unless the two
 *   branches are duplicate keys
 * - the xor between two nodes has *at least* the split bit set, possibly more
 * - the split bit is always strictly smaller for a node than for its parent,
 *   which implies that the xor between the keys of the lowest level node is
 *   always smaller than the xor between a higher level node. Hence the xor
 *   between the branches of a regular leaf is always strictly larger than the
 *   xor of its parent node's branches if this node is different, since the
 *   leaf is associated with a higher level node which has at least one higher
 *   level branch. The first leaf doesn't validate this but is handled by the
 *   rules below.
 * - during the descent, the node corresponding to a leaf is always visited
 *   before the leaf, unless it's the first inserted, nodeless leaf.
 * - the first key is the only one without any node, and it has both its
 *   branches pointing to itself during insertion to detect it (i.e. xor==0).
 * - a leaf is always present as a node on the path from the root, except for
 *   the inserted first key which has no node, and is recognizable by its two
 *   branches pointing to itself.
 * - a consequence of the rules above is that a non-first leaf appearing below
 *   a node will necessarily have an associated node with a split bit equal to
 *   or greater than the node's split bit.
 * - another consequence is that below a node, the split bits are different for
 *   each branches since both of them are already present above the node, thus
 *   at different levels, so their respective XOR values will be different.
 * - since all nodes in a given path have a different split bit, if a leaf has
 *   the same split bit as its parent node, it is necessary its associated leaf
 *
 * When descending along the tree, it is possible to know that a search key is
 * not present, because its XOR with both of the branches is stricly higher
 * than the inter-branch XOR. The reason is simple : the inter-branch XOR will
 * have its highest bit set indicating the split bit. Since it's the bit that
 * differs between the two branches, the key cannot have it both set and
 * cleared when comparing to the branch values. So xoring the key with both
 * branches will emit a higher bit only when the key's bit differs from both
 * branches' similar bit. Thus, the following equation :
 *      (XOR(key, L) > XOR(L, R)) && (XOR(key, R) > XOR(L, R))
 * is only true when the key should be placed above that node. Since the key
 * has a higher bit which differs from the node, either it has it set and the
 * node has it clear (same for both branches), or it has it clear and the node
 * has it set for both branches. For this reason it's enough to compare the key
 * with any node when the equation above is true, to know if it ought to be
 * present on the left or on the right side. This is useful for insertion and
 * for range lookups.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cbatree.h"
#include "cbatree-prv.h"

// to be redone: compare the 3 strings as long as they match, then
// fall back to different labels depending on the non-matching one.
//static cmp3str(const unsigned char *k, const unsigned char *l, const unsigned char *r,
//	       int *ret_llen, int *ret_rlen, int *ret_xlen)
//{
//	unsigned char kl, kr, lr;
//
//	int beg = 0;
//	int llen = 0, rlen = 0, xlen = 0;
//
//	/* skip known and identical bits. We stop at the first different byte
//	 * or at the first zero we encounter on either side.
//	 */
//	while (1) {
//		unsigned char cl, cr, ck;
//		cl = *l;
//		cr = *r;
//		ck = *k;
//
//		kl = cl ^ ck;
//		kr = cr ^ ck;
//		lr = cl ^ cr;
//
//		c = a[beg];
//		d = b[beg];
//		beg++;
//
//		c ^= d;
//		if (c)
//			break;
//		if (!d)
//			return -1;
//	}
//	/* OK now we know that a and b differ at byte <beg>, or that both are zero.
//	 * We have to find what bit is differing and report it as the number of
//	 * identical bits. Note that low bit numbers are assigned to high positions
//	 * in the byte, as we compare them as strings.
//	 */
//	return (beg << 3) - flsnz8(c);
//
//}

/* Inserts node <node> into unique tree <tree> based on its key that
 * immediately follows the node. Returns the inserted node or the one
 * that already contains the same key.
 */
struct cba_node *cba_insert_st(struct cba_node **root, struct cba_node *node)
{
	const void *key = &container_of(node, struct cba_st, node)->key;

	return _cbau_insert(root, node, CB_KT_ST, key);
}

/* return the first node or NULL if not found. */
struct cba_node *cba_first_st(struct cba_node **root)
{
	return _cbau_first(root, CB_KT_ST);
}

/* return the last node or NULL if not found. */
struct cba_node *cba_last_st(struct cba_node **root)
{
	return _cbau_last(root, CB_KT_ST);
}

/* look up the specified key, and returns either the node containing it, or
 * NULL if not found.
 */
struct cba_node *cba_lookup_st(struct cba_node **root, const void *key)
{
	return _cbau_lookup(root, CB_KT_ST, key);
}

/* search for the next node after the specified one, and return it, or NULL if
 * not found. The approach consists in looking up that node, recalling the last
 * time a left turn was made, and returning the first node along the right
 * branch at that fork.
 */
struct cba_node *cba_next_st(struct cba_node **root, struct cba_node *node)
{
	const void *key = &container_of(node, struct cba_st, node)->key;

	return _cbau_next(root, CB_KT_ST, key);
}

/* search for the prev node before the specified one, and return it, or NULL if
 * not found. The approach consists in looking up that node, recalling the last
 * time a right turn was made, and returning the last node along the left
 * branch at that fork.
 */
struct cba_node *cba_prev_st(struct cba_node **root, struct cba_node *node)
{
	const void *key = &container_of(node, struct cba_st, node)->key;

	return _cbau_prev(root, CB_KT_ST, key);
}

/* look up the specified node with its key and deletes it if found, and in any
 * case, returns the node.
 */
struct cba_node *cba_delete_st(struct cba_node **root, struct cba_node *node)
{
	const void *key = &container_of(node, struct cba_st, node)->key;

	return _cbau_delete(root, node, CB_KT_ST, key);
}

/* look up the specified key, and detaches it and returns it if found, or NULL
 * if not found.
 */
struct cba_node *cba_pick_st(struct cba_node **root, const void *key)
{
	return _cbau_delete(root, NULL, CB_KT_ST, key);
}

#if 0
/* default node dump function */
static void cbast_default_dump_node(struct cba_node *node, int level, const void *ctx)
{
	struct cba_st *key = container_of(node, struct cba_st, node);
	mb pxor, lxor, rxor;

	/* xor of the keys of the two lower branches */
	pxor = container_of(__cba_clrtag(node->b[0]), struct cba_st, node)->key ^
		container_of(__cba_clrtag(node->b[1]), struct cba_st, node)->key;

	printf("  \"%lx_n\" [label=\"%lx\\nlev=%d\\nkey=%u\" fillcolor=\"lightskyblue1\"%s];\n",
	       (long)node, (long)node, level, key->key, (ctx == node) ? " color=red" : "");

	/* xor of the keys of the left branch's lower branches */
	lxor = container_of(__cba_clrtag(((struct cba_node*)__cba_clrtag(node->b[0]))->b[0]), struct cba_st, node)->key ^
		container_of(__cba_clrtag(((struct cba_node*)__cba_clrtag(node->b[0]))->b[1]), struct cba_st, node)->key;

	printf("  \"%lx_n\" -> \"%lx_%c\" [label=\"L\" arrowsize=0.66 %s];\n",
	       (long)node, (long)__cba_clrtag(node->b[0]),
	       (((long)node->b[0] & 1) || (lxor < pxor && ((struct cba_node*)node->b[0])->b[0] != ((struct cba_node*)node->b[0])->b[1])) ? 'n' : 'l',
	       (node == __cba_clrtag(node->b[0])) ? " dir=both" : "");

	/* xor of the keys of the right branch's lower branches */
	rxor = container_of(__cba_clrtag(((struct cba_node*)__cba_clrtag(node->b[1]))->b[0]), struct cba_st, node)->key ^
		container_of(__cba_clrtag(((struct cba_node*)__cba_clrtag(node->b[1]))->b[1]), struct cba_st, node)->key;

	printf("  \"%lx_n\" -> \"%lx_%c\" [label=\"R\" arrowsize=0.66 %s];\n",
	       (long)node, (long)__cba_clrtag(node->b[1]),
	       (((long)node->b[1] & 1) || (rxor < pxor && ((struct cba_node*)node->b[1])->b[0] != ((struct cba_node*)node->b[1])->b[1])) ? 'n' : 'l',
	       (node == __cba_clrtag(node->b[1])) ? " dir=both" : "");
}

/* default leaf dump function */
static void cbast_default_dump_leaf(struct cba_node *node, int level, const void *ctx)
{
	struct cba_st *key = container_of(node, struct cba_st, node);

	if (node->b[0] == node->b[1])
		printf("  \"%lx_l\" [label=\"%lx\\nlev=%d\\nkey=%u\\n\" fillcolor=\"green\"%s];\n",
		       (long)node, (long)node, level, key->key, (ctx == node) ? " color=red" : "");
	else
		printf("  \"%lx_l\" [label=\"%lx\\nlev=%d\\nkey=%u\\n\" fillcolor=\"yellow\"%s];\n",
		       (long)node, (long)node, level, key->key, (ctx == node) ? " color=red" : "");
}

/* Dumps a tree through the specified callbacks. */
void *cba_dump_tree_st(struct cba_node *node, mb pxor, void *last,
			int level,
			void (*node_dump)(struct cba_node *node, int level, const void *ctx),
			void (*leaf_dump)(struct cba_node *node, int level, const void *ctx),
			const void *ctx)
{
	mb xor;

	if (!node) /* empty tree */
		return node;

	//CBADBG("node=%p level=%d key=%u l=%p r=%p\n", node, level, *(unsigned *)((char*)(node)+16), node->b[0], node->b[1]);

	if (level < 0) {
		/* we're inside a dup tree. Tagged pointers indicate nodes,
		 * untagged ones leaves.
		 */
		level--;
		if (__cba_tagged(node->b[0])) {
		  last = cba_dump_tree_st(__cba_untag(node->b[0]), 0, last, level, node_dump, leaf_dump, ctx);
			if (node_dump)
			  node_dump(__cba_untag(node->b[0]), level, ctx);
		} else if (leaf_dump)
			leaf_dump(node->b[0], level, ctx);

		if (__cba_tagged(node->b[1])) {
			last = cba_dump_tree_st(__cba_untag(node->b[1]), 0, last, level, node_dump, leaf_dump, ctx);
			if (node_dump)
				node_dump(__cba_untag(node->b[1]), level, ctx);
		} else if (leaf_dump)
			leaf_dump(node->b[1], level, ctx);
		return node;
	}

	/* regular nodes, all branches are canonical */

	if (node->b[0] == node->b[1]) {
		/* first inserted leaf */
		if (leaf_dump)
			leaf_dump(node, level, ctx);
		return node;
	}

	if (0/*__cba_is_dup(node)*/) {
		if (node_dump)
			node_dump(node, -1, ctx);
		return cba_dump_tree_st(node, 0, last, -1, node_dump, leaf_dump, ctx);
	}

	xor = ((struct cba_st*)node->b[0])->key ^ ((struct cba_st*)node->b[1])->key;
	if (pxor && xor >= pxor) {
		/* that's a leaf */
		if (leaf_dump)
			leaf_dump(node, level, ctx);
		return node;
	}

	if (!xor) {
		/* start of a dup */
		if (node_dump)
			node_dump(node, -1, ctx);
		return cba_dump_tree_st(node, 0, last, -1, node_dump, leaf_dump, ctx);
	}

	/* that's a regular node */
	if (node_dump)
		node_dump(node, level, ctx);

	last = cba_dump_tree_st(node->b[0], xor, last, level + 1, node_dump, leaf_dump, ctx);
	return cba_dump_tree_st(node->b[1], xor, last, level + 1, node_dump, leaf_dump, ctx);
}

/* dumps a cba_st tree using the default functions above. If a node matches
 * <ctx>, this one will be highlighted in red.
 */
void cbast_default_dump(struct cba_node **cba_root, const char *label, const void *ctx)
{
	struct cba_node *node;

	printf("\ndigraph cba_tree_st {\n"
	       "  fontname=\"fixed\";\n"
	       "  fontsize=8\n"
	       "  label=\"%s\"\n"
	       "", label);

	printf("  node [fontname=\"fixed\" fontsize=8 shape=\"box\" style=\"filled\" color=\"black\" fillcolor=\"white\"];\n"
	       "  edge [fontname=\"fixed\" fontsize=8 style=\"solid\" color=\"magenta\" dir=\"forward\"];\n"
	       "  \"%lx_n\" [label=\"root\\n%lx\"]\n", (long)cba_root, (long)cba_root);

	node = *cba_root;
	if (node) {
		/* under the root we've either a node or the first leaf */
		printf("  \"%lx_n\" -> \"%lx_%c\" [label=\"B\" arrowsize=0.66];\n",
		       (long)cba_root, (long)node,
		       (node->b[0] == node->b[1]) ? 'l' : 'n');
	}

	cba_dump_tree_st(*cba_root, 0, NULL, 0, cbast_default_dump_node, cbast_default_dump_leaf, ctx);

	printf("}\n");
}
#endif