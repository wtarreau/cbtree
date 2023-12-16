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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cbatree.h"
#include "cbatree-prv.h"

struct cba_node *cba_insert_st(struct cba_node **root, struct cba_node *node)
{
	const typeof(((struct cba_st*)0)->key) *key = &container_of(node, struct cba_st, node)->key;
	struct cba_node **parent;
	struct cba_node *ret;
	int nside;

	if (!*root) {
		/* empty tree, insert a leaf only */
		node->b[0] = node->b[1] = node;
		*root = node;
		return node;
	}

	ret = cbau_descend_st(root, CB_WM_KEY, node, key, &nside, &parent, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	if (ret == node) {
		node->b[nside] = node;
		node->b[!nside] = *parent;
		*parent = ret;
	}
	return ret;
}

/* return the first node or NULL if not found. */
struct cba_node *cba_first_st(struct cba_node **root)
{
	if (!*root)
		return NULL;

	return cbau_descend_st(root, CB_WM_FST, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

/* return the last node or NULL if not found. */
struct cba_node *cba_last_st(struct cba_node **root)
{
	if (!*root)
		return NULL;

	return cbau_descend_st(root, CB_WM_LST, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

/* look up the specified key, and returns either the node containing it, or
 * NULL if not found.
 */
struct cba_node *cba_lookup_st(struct cba_node **root, const void *key)
{
	if (!*root)
		return NULL;

	return cbau_descend_st(root, CB_WM_KEY, NULL, key, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

/* search for the next node after the specified one, and return it, or NULL if
 * not found. The approach consists in looking up that node, recalling the last
 * time a left turn was made, and returning the first node along the right
 * branch at that fork.
 */
struct cba_node *cba_next_st(struct cba_node **root, struct cba_node *node)
{
	const typeof(((struct cba_st*)0)->key) *key = &container_of(node, struct cba_st, node)->key;
	struct cba_node **right_branch = NULL;

	if (!*root)
		return NULL;

	cbau_descend_st(root, CB_WM_KEY, NULL, key, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &right_branch);
	if (!right_branch)
		return NULL;
	return cbau_descend_st(right_branch, CB_WM_NXT, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

/* search for the prev node before the specified one, and return it, or NULL if
 * not found. The approach consists in looking up that node, recalling the last
 * time a right turn was made, and returning the last node along the left
 * branch at that fork.
 */
struct cba_node *cba_prev_st(struct cba_node **root, struct cba_node *node)
{
	const typeof(((struct cba_st*)0)->key) *key = &container_of(node, struct cba_st, node)->key;
	struct cba_node **left_branch = NULL;

	if (!*root)
		return NULL;

	cbau_descend_st(root, CB_WM_KEY, NULL, key, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &left_branch, NULL);
	if (!left_branch)
		return NULL;
	return cbau_descend_st(left_branch, CB_WM_PRV, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

/* look up the specified node with its key and deletes it if found, and in any
 * case, returns the node.
 */
struct cba_node *cba_delete_st(struct cba_node **root, struct cba_node *node)
{
	const typeof(((struct cba_st*)0)->key) *key = &container_of(node, struct cba_st, node)->key;
	struct cba_node *lparent, *nparent, *gparent, *sibling;
	int lpside, npside, gpside;
	struct cba_node *ret;

	if (!node->b[0]) {
		/* NULL on a branch means the node is not in the tree */
		return node;
	}

	if (!*root) {
		/* empty tree, the node cannot be there */
		return node;
	}

	ret = cbau_descend_st(root, CB_WM_KEY, NULL, key, NULL, NULL, &lparent, &lpside, &nparent, &npside, &gparent, &gpside, NULL, NULL);
	if (ret == node) {
		//CBADBG("root=%p ret=%p l=%p[%d] n=%p[%d] g=%p[%d]\n", root, ret, lparent, lpside, nparent, npside, gparent, gpside);

		if (&lparent->b[0] == root) {
			/* there was a single entry, this one */
			*root = NULL;
			goto done;
		}
		//printf("g=%p\n", gparent);

		/* then we necessarily have a gparent */
		sibling = lpside ? lparent->b[0] : lparent->b[1];
		gparent->b[gpside] = sibling;

		if (lparent == node) {
			/* we're removing the leaf and node together, nothing
			 * more to do.
			 */
			goto done;
		}

		if (node->b[0] == node->b[1]) {
			/* we're removing the node-less item, the parent will
			 * take this role.
			 */
			lparent->b[0] = lparent->b[1] = lparent;
			goto done;
		}

		/* more complicated, the node was split from the leaf, we have
		 * to find a spare one to switch it. The parent node is not
		 * needed anymore so we can reuse it.
		 */
		lparent->b[0] = node->b[0];
		lparent->b[1] = node->b[1];
		nparent->b[npside] = lparent;
	}
done:
	return ret;
}

/* look up the specified key, and detaches it and returns it if found, or NULL
 * if not found.
 */
struct cba_node *cba_pick_st(struct cba_node **root, const void *key)
{
	struct cba_node *lparent, *nparent, *gparent/*, *sibling*/;
	int lpside, npside, gpside;
	struct cba_node *ret;

	if (!*root)
		return NULL;

	//if (key == 425144) printf("%d: k=%u\n", __LINE__, key);

	ret = cbau_descend_st(root, CB_WM_KEY, NULL, key, NULL, NULL, &lparent, &lpside, &nparent, &npside, &gparent, &gpside, NULL, NULL);

	//if (key == 425144) printf("%d: k=%u ret=%p\n", __LINE__, key, ret);

	if (ret) {
		struct cba_st *p = container_of(ret, struct cba_st, node);

		if (p->key != key)
			abort();

		//CBADBG("root=%p ret=%p l=%p[%d] n=%p[%d] g=%p[%d]\n", root, ret, lparent, lpside, nparent, npside, gparent, gpside);

		if (&lparent->b[0] == root) {
			/* there was a single entry, this one */
			*root = NULL;
			//if (key == 425144) printf("%d: k=%u ret=%p\n", __LINE__, key, ret);
			goto done;
		}
		//printf("g=%p\n", gparent);

		/* then we necessarily have a gparent */
		//sibling = lpside ? lparent->b[0] : lparent->b[1];
		//gparent->b[gpside] = sibling;

		gparent->b[gpside] = lparent->b[!lpside];

		if (lparent == ret) {
			/* we're removing the leaf and node together, nothing
			 * more to do.
			 */
			//if (key == 425144) printf("%d: k=%u ret=%p\n", __LINE__, key, ret);
			goto done;
		}

		if (ret->b[0] == ret->b[1]) {
			/* we're removing the node-less item, the parent will
			 * take this role.
			 */
			lparent->b[0] = lparent->b[1] = lparent;
			//if (key == 425144) printf("%d: k=%u ret=%p\n", __LINE__, key, ret);
			goto done;
		}

		/* more complicated, the node was split from the leaf, we have
		 * to find a spare one to switch it. The parent node is not
		 * needed anymore so we can reuse it.
		 */
		//if (key == 425144) printf("%d: k=%u ret=%p lp=%p np=%p gp=%p\n", __LINE__, key, ret, lparent, nparent, gparent);
		lparent->b[0] = ret->b[0];
		lparent->b[1] = ret->b[1];
		nparent->b[npside] = lparent;
	}
done:
	//if (key == 425144) printf("%d: k=%u ret=%p\n", __LINE__, key, ret);
	return ret;
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
