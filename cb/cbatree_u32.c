/*
 * Compact Binary Trees - exported functions for operations on u32 keys
 *
 * Copyright (C) 2014-2021 Willy Tarreau - w@1wt.eu
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
#include "cbatree.h"

/* this structure is aliased to the common cba node during u32 operations */
struct cba_u32 {
	struct cba_node node;
	u32 key;
};

/* Generic tree descent function. It must absolutely be inlined so that the
 * compiler can eliminate the tests related to the various return pointers,
 * which must either point to a local variable in the caller, or be NULL.
 * It must not be called with an empty tree, it's the caller business to
 * deal with this special case. It returns in ret_root the location of the
 * pointer to the leaf (i.e. where we have to insert ourselves). The integer
 * pointed to by ret_nside will contain the side the leaf should occupy at
 * its own node, with the sibling being *ret_root.
 */
static inline __attribute__((always_inline))
struct cba_node *cbau_descend_u32(/*const*/ struct cba_node **root,
				  /*const*/ struct cba_node *node,
				  int *ret_nside,
				  struct cba_node ***ret_root,
				  struct cba_node **ret_lparent,
				  int *ret_lpside,
				  struct cba_node **ret_nparent,
				  int *ret_npside,
				  struct cba_node **ret_gparent,
				  int *ret_gpside)
{
	struct cba_u32 *p, *l, *r;
	u32 pxor = ~0; // make sure we don't run the first test.
	u32 key = container_of(node, struct cba_u32, node)->key;
	struct cba_node *gparent = NULL;
	struct cba_node *nparent = NULL;
	struct cba_node *lparent;
	int gpside = 0; // side on the grand parent
	int npside = 0;  // side on the node's parent
	int lpside = 0;  // side on the leaf's parent

	/* When exiting the loop, pxor will be zero for nodes and first leaf,
	 * or non-zero for a leaf.
	 */

	/* the parent will be the (possibly virtual) node so that
	 * &lparent->l == root.
	 */
	lparent = container_of(root, struct cba_node, b[0]);
	gparent = nparent = lparent;

	/* the previous xor is initialized to the largest possible inter-branch
	 * value so that it can never match on the first test as we want to use
	 * it to detect a leaf vs node.
	 */
	while (1) {
		p = container_of(*root, struct cba_u32, node);

		/* neither pointer is tagged */
		l = container_of(p->node.b[0], struct cba_u32, node);
		r = container_of(p->node.b[1], struct cba_u32, node);

		/* two equal pointers identifies the nodeless leaf */
		if (l == r) {
			//fprintf(stderr, "key %u break at %d\n", key, __LINE__);
			break;
		}

		/* so that's either a node or a leaf. Each leaf we visit had
		 * its node part already visited. The only way to distinguish
		 * them is that the inter-branch xor of the leaf will be the
		 * node's one, and will necessarily be larger than the previous
		 * node's xor if the node is above (we've already checked for
		 * direct descendent below). Said differently, if an inter-
		 * branch xor is strictly larger than the previous one, it
		 * necessarily is the one of an upper node, so what we're
		 * seeing cannot be the node, hence it's the leaf.
		 */
		if ((l->key ^ r->key) > pxor) { // test using 2 4 6 4
			/* this is a leaf */
			//fprintf(stderr, "key %u break at %d\n", key, __LINE__);
			break;
		}

		pxor = l->key ^ r->key;

		/* check the split bit */
		if ((key ^ l->key) > pxor && (key ^ r->key) > pxor) {
			/* can't go lower, the node must be inserted above p
			 * (which is then necessarily a node). We also know
			 * that (key != p->key) because p->key differs from at
			 * least one of its subkeys by a higher bit than the
			 * split bit, so lookups must fail here.
			 */
			//fprintf(stderr, "key %u break at %d\n", key, __LINE__);
			break;
		}

		/* here we're guaranteed to be above a node. If this is the
		 * same node as the one we're looking for, let's store the
		 * parent as the node's parent.
		 */
		if (node == &p->node) {
			nparent = lparent;
			npside  = lpside;
		}

		/* shift all copies by one */
		gparent = lparent;
		gpside = lpside;
		lparent = &p->node;

		if ((key ^ l->key) < (key ^ r->key)) {
			lpside = 0;
			root = &p->node.b[0];
		}
		else {
			lpside = 1;
			root = &p->node.b[1];
		}

		if (p == container_of(*root, struct cba_u32, node)) {
			//fprintf(stderr, "key %u break at %d\n", key, __LINE__);
			/* loops over itself, it's a leaf */
			break;
		}
	}

	/* update the pointers needed for modifications (insert, delete) */
	if (ret_nside)
		*ret_nside = key >= p->key;

	if (ret_root)
		*ret_root = root;

	/* info needed by delete */
	if (ret_lpside)
		*ret_lpside = lpside;

	if (ret_lparent)
		*ret_lparent = lparent;

	if (ret_npside)
		*ret_npside = npside;

	if (ret_nparent)
		*ret_nparent = nparent;

	if (ret_gpside)
		*ret_gpside = gpside;

	if (ret_gparent)
		*ret_gparent = gparent;

	/* For lookups, an equal value means an instant return. For insertions,
	 * it is the same, we want to return the previously existing value so
	 * that the caller can decide what to do. For deletion, we also want to
	 * return the pointer that's about to be deleted.
	 */
	if (key == p->key)
		return &p->node;//*root;  both are the same, but p->node should be cheaper

//	/* We're going to insert <node> above leaf <p> and below <root>. It's
//	 * possible that <p> is the first inserted node, or that it's any other
//	 * regular node or leaf. Therefore we don't care about pointer tagging.
//	 * We need to know two things :
//	 *  - whether <p> is left or right on <root>, to unlink it properly and
//	 *    to take its place
//	 *  - whether we attach <p> left or right below us
//	 */
//	if (!ret_root && !ret_lparent && key == p->key) {
//		/* for lookups this is sufficient. For insert the caller can
//		 * verify that the result is not node hence a conflicting value
//		 * already existed. We do not make more efforts for now towards
//		 * duplicates.
//		 */
//		return *root;
//	}

	/* lookups and deletes fail here */
	/* plain lookups just stop here */
	if (!ret_root)
		return NULL;

	/* inserts return the node we expect to insert */
	//printf("descent insert: node=%p key=%d root=%p *root=%p p->node=%p\n", node, key, root, *root, &p->node);
	return node;
	//return &p->node;
}

struct cba_node *cba_insert_u32(struct cba_node **root, struct cba_node *node)
{
	struct cba_node **parent;
	struct cba_node *ret;
	int nside;

	if (!*root) {
		/* empty tree, insert a leaf only */
		node->b[0] = node->b[1] = node;
		*root = node;
		return node;
	}

	ret = cbau_descend_u32(root, node, &nside, &parent, NULL, NULL, NULL, NULL, NULL, NULL);

	if (ret == node) {
		if (!nside) {
			node->b[0] = node;
			node->b[1] = *parent;
		}
		else {
			node->b[0] = *parent;
			node->b[1] = node;
		}
		*parent = ret;
	}
	return ret;
}

/* look up the specified key, and returns either the node containing it, or
 * NULL if not found.
 */
struct cba_node *cba_lookup_u32(struct cba_node **root, u32 key)
{
	u32 key_back = key;
	/*const*/ struct cba_node *node = &container_of(&key_back, struct cba_u32, key)->node;

	if (!*root)
		return NULL;

	return cbau_descend_u32(root, node, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

/* look up the specified node with its key and deletes it if found, and in any
 * case, returns the node.
 */
struct cba_node *cba_delete_u32(struct cba_node **root, struct cba_node *node)
{
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

	ret = cbau_descend_u32(root, node, NULL, NULL, &lparent, &lpside, &nparent, &npside, &gparent, &gpside);
	if (ret == node) {
		//fprintf(stderr, "root=%p ret=%p l=%p[%d] n=%p[%d] g=%p[%d]\n", root, ret, lparent, lpside, nparent, npside, gparent, gpside);

		if (&lparent->b[0] == root) {
			/* there was a single entry, this one */
			*root = NULL;
			goto done;
		}
		//printf("g=%p\n", gparent);

		/* then we necessarily have a gparent */
		sibling = lpside ? lparent->b[0] : lparent->b[1];
		if (!gpside)
			gparent->b[0] = sibling;
		else
			gparent->b[1] = sibling;

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

		if (!npside)
			nparent->b[0] = lparent;
		else
			nparent->b[1] = lparent;
	}
done:
	return ret;
}

///* returns the highest node which is less than or equal to data. This is
// * typically used to know what memory area <data> belongs to.
// */
//struct cba_node *cba_lookup_le(struct cba_node **root, void *data)
//{
//	struct cba_node *p, *last_r;
//	u32 pxor;
//
//	pxor = 0;
//	p = *root;
//
//	if (!p)
//		return p;
//
//	last_r = NULL;
//	while (1) {
//		if (!p->l || (xorptr(p->l, p->r) >= pxor && pxor != 0)) {
//			/* first leaf inserted, or regular leaf. Either
//			 * the entry fits our expectations or we have to
//			 * roll back and go down the opposite direction.
//			 */
//			if ((u32)p > (u32)data)
//				break;
//			return p;
//		}
//
//		pxor = xorptr(p->l, p->r);
//		if (xorptr(data, p->l) > pxor && xorptr(data, p->r) > pxor) {
//			/* The difference between the looked up key and the branches
//			 * is higher than the difference between the branches, which
//			 * means that the key ought to have been found upper in the
//			 * chain. Since we won't find the key below, either we have
//			 * a chance to find the largest inferior one below and we
//			 * walk down, or we need to rewind.
//			 */
//			if ((u32)p->l > (u32)data)
//				break;
//
//			p = p->r;
//			goto walkdown;
//		}
//
//		if (xorptr(data, p->l) < xorptr(data, p->r)) {
//			root = &p->l;
//		}
//		else {
//			last_r = p;
//			root = &p->r;
//		}
//
//		p = *root;
//	}
//
//	/* first roll back to last node where we turned right, and go down left
//	 * or stop at first leaf if any. If we only went down left, we already
//	 * found the smallest key so none will suit us.
//	 */
//	if (!last_r)
//		return NULL;
//
//	pxor = xorptr(last_r->l, last_r->r);
//	p = last_r->l;
//
// walkdown:
//	/* switch to the other branch last time we turned right */
//	while (p->r) {
//		if (xorptr(p->l, p->r) >= pxor)
//			break;
//		pxor = xorptr(p->l, p->r);
//		p = p->r;
//	}
//
//	if ((u32)p > (u32)data)
//		return NULL;
//	return p;
//}
//
///* returns the note which equals <data> or NULL if <data> is not in the tree */
//struct cba_node *cba_lookup(struct cba_node **root, void *data)
//{
//	struct cba_node *p;
//	u32 pxor;
//
//	pxor = 0;
//	p = *root;
//
//	if (!p)
//		return p;
//
//	while (1) {
//		if (!p->l || (xorptr(p->l, p->r) >= pxor && pxor != 0)) {
//			/* first leaf inserted, or regular leaf */
//			if ((u32)p != (u32)data)
//				p = NULL;
//			break;
//		}
//
//		pxor = xorptr(p->l, p->r);
//		if (xorptr(data, p->l) > pxor && xorptr(data, p->r) > pxor) {
//			/* The difference between the looked up key and the branches
//			 * is higher than the difference between the branches, which
//			 * means that the key ought to have been found upper in the
//			 * chain.
//			 */
//			p = NULL;
//			break;
//		}
//
//		if (xorptr(data, p->l) < xorptr(data, p->r))
//			root = &p->l;
//		else
//			root = &p->r;
//
//		p = *root;
//	}
//	return p;
//}
//
///* returns the lowest node which is greater than or equal to data. This is
// * typically used to know the distance between <data> and the next memory
// * area.
// */
//struct cba_node *cba_lookup_ge(struct cba_node **root, void *data)
//{
//	struct cba_node *p, *last_l;
//	u32 pxor;
//
//	pxor = 0;
//	p = *root;
//
//	if (!p)
//		return p;
//
//	last_l = NULL;
//	while (1) {
//		if (!p->l || (xorptr(p->l, p->r) >= pxor && pxor != 0)) {
//			/* first leaf inserted, or regular leaf. Either
//			 * the entry fits our expectations or we have to
//			 * roll back and go down the opposite direction.
//			 */
//			if ((u32)p < (u32)data)
//				break;
//			return p;
//		}
//
//		pxor = xorptr(p->l, p->r);
//		if (xorptr(data, p->l) > pxor && xorptr(data, p->r) > pxor) {
//			/* The difference between the looked up key and the branches
//			 * is higher than the difference between the branches, which
//			 * means that the key ought to have been found upper in the
//			 * chain. Since we won't find the key below, either we have
//			 * a chance to find the smallest superior one below and we
//			 * walk down, or we need to rewind.
//			 */
//			if ((u32)p->l < (u32)data)
//				break;
//
//			p = p->l;
//			goto walkdown;
//		}
//
//		if (xorptr(data, p->l) < xorptr(data, p->r)) {
//			last_l = p;
//			root = &p->l;
//		}
//		else {
//			root = &p->r;
//		}
//
//		p = *root;
//	}
//
//	/* first roll back to last node where we turned right, and go down left
//	 * or stop at first leaf if any. If we only went down left, we already
//	 * found the smallest key so none will suit us.
//	 */
//	if (!last_l)
//		return NULL;
//
//	pxor = xorptr(last_l->l, last_l->r);
//	p = last_l->r;
//
// walkdown:
//	/* switch to the other branch last time we turned left */
//	while (p->l) {
//		if (xorptr(p->l, p->r) >= pxor)
//			break;
//		pxor = xorptr(p->l, p->r);
//		p = p->l;
//	}
//
//	if ((u32)p < (u32)data)
//		return NULL;
//	return p;
//}

/* Dumps a tree through the specified callbacks. */
void *cba_dump_tree_u32(struct cba_node *node, u32 pxor, void *last,
			int level,
			void (*node_dump)(struct cba_node *node, int level),
			void (*leaf_dump)(struct cba_node *node, int level))
{
	u32 xor;

	if (!node) /* empty tree */
		return node;

	fprintf(stderr, "node=%p level=%d key=%u l=%p r=%p\n", node, level, *(unsigned *)((char*)(node)+16), node->b[0], node->b[1]);

	if (level < 0) {
		/* we're inside a dup tree. Tagged pointers indicate nodes,
		 * untagged ones leaves.
		 */
		level--;
		if (__cba_tagged(node->b[0])) {
			last = cba_dump_tree_u32(__cba_untag(node->b[0]), 0, last, level, node_dump, leaf_dump);
			if (node_dump)
				node_dump(__cba_untag(node->b[0]), level);
		} else if (leaf_dump)
			leaf_dump(node->b[0], level);

		if (__cba_tagged(node->b[1])) {
			last = cba_dump_tree_u32(__cba_untag(node->b[1]), 0, last, level, node_dump, leaf_dump);
			if (node_dump)
				node_dump(__cba_untag(node->b[1]), level);
		} else if (leaf_dump)
			leaf_dump(node->b[1], level);
		return node;
	}

	/* regular nodes, all branches are canonical */

	if (node->b[0] == node->b[1]) {
		/* first inserted leaf */
		if (leaf_dump)
			leaf_dump(node, level);
		return node;
	}

	if (0/*__cba_is_dup(node)*/) {
		if (node_dump)
			node_dump(node, -1);
		return cba_dump_tree_u32(node, 0, last, -1, node_dump, leaf_dump);
	}

	xor = ((struct cba_u32*)node->b[0])->key ^ ((struct cba_u32*)node->b[1])->key;
	if (pxor && xor >= pxor) {
		/* that's a leaf */
		if (leaf_dump)
			leaf_dump(node, level);
		return node;
	}

	if (!xor) {
		/* start of a dup */
		if (node_dump)
			node_dump(node, -1);
		return cba_dump_tree_u32(node, 0, last, -1, node_dump, leaf_dump);
	}

	/* that's a regular node */
	if (node_dump)
		node_dump(node, level);

	last = cba_dump_tree_u32(node->b[0], xor, last, level + 1, node_dump, leaf_dump);
	return cba_dump_tree_u32(node->b[1], xor, last, level + 1, node_dump, leaf_dump);
}
