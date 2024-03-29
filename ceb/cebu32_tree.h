/*
 * Compact Elastic Binary Trees - exported functions operating on u32 keys
 *
 * Copyright (C) 2014-2024 Willy Tarreau - w@1wt.eu
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

#include "cebtree.h"

struct ceb_node *cebu32_insert(struct ceb_node **root, struct ceb_node *node);
struct ceb_node *cebu32_first(struct ceb_node **root);
struct ceb_node *cebu32_last(struct ceb_node **root);
struct ceb_node *cebu32_lookup(struct ceb_node **root, uint32_t key);
struct ceb_node *cebu32_lookup_le(struct ceb_node **root, uint32_t key);
struct ceb_node *cebu32_lookup_lt(struct ceb_node **root, uint32_t key);
struct ceb_node *cebu32_lookup_ge(struct ceb_node **root, uint32_t key);
struct ceb_node *cebu32_lookup_gt(struct ceb_node **root, uint32_t key);
struct ceb_node *cebu32_next(struct ceb_node **root, struct ceb_node *node);
struct ceb_node *cebu32_prev(struct ceb_node **root, struct ceb_node *node);
struct ceb_node *cebu32_delete(struct ceb_node **root, struct ceb_node *node);
struct ceb_node *cebu32_pick(struct ceb_node **root, uint32_t key);
void *cebu32_dump_tree(struct ceb_node *node, u32 pxor, void *last,
                       int level,
                       void (*node_dump)(struct ceb_node *node, int level, const void *ctx),
                       void (*leaf_dump)(struct ceb_node *node, int level, const void *ctx),
                       const void *ctx);
void cebu32_default_dump(struct ceb_node **ceb_root, const char *label, const void *ctx);
