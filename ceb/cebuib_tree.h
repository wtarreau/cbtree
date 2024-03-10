/*
 * Compact Elastic Binary Trees - exported functions operating on indirect blocks
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

struct ceb_node *cebuib_insert(struct ceb_node **root, struct ceb_node *node, size_t len);
struct ceb_node *cebuib_first(struct ceb_node **root);
struct ceb_node *cebuib_last(struct ceb_node **root);
struct ceb_node *cebuib_lookup(struct ceb_node **root, const void *key, size_t len);
struct ceb_node *cebuib_lookup_le(struct ceb_node **root, const void *key, size_t len);
struct ceb_node *cebuib_lookup_lt(struct ceb_node **root, const void *key, size_t len);
struct ceb_node *cebuib_lookup_ge(struct ceb_node **root, const void *key, size_t len);
struct ceb_node *cebuib_lookup_gt(struct ceb_node **root, const void *key, size_t len);
struct ceb_node *cebuib_next(struct ceb_node **root, struct ceb_node *node, size_t len);
struct ceb_node *cebuib_prev(struct ceb_node **root, struct ceb_node *node, size_t len);
struct ceb_node *cebuib_delete(struct ceb_node **root, struct ceb_node *node, size_t len);
struct ceb_node *cebuib_pick(struct ceb_node **root, const void *key, size_t len);
