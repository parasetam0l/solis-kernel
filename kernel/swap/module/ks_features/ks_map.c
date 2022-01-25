#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include "ks_map.h"

struct entry {
	struct rb_node node;
	void *data;
};

static inline void *entry_data(struct entry *entry)
{
	return entry->data;
}

static struct entry *alloc_entry(struct map *map, void *data)
{
	struct entry *entry = kmalloc(sizeof(*entry), GFP_ATOMIC);

	if (entry) {
		entry->data = data;
		RB_CLEAR_NODE(&entry->node);
	}

	return entry;
}

static void *free_entry(struct map *map, struct entry *entry)
{
	void *data = entry_data(entry);

	kfree(entry);

	return data;
}

static struct entry *__search(struct map *map, void *key)
{
	struct rb_root *root = &map->root;
	struct rb_node *node = root->rb_node;
	key_func_t key_f = map->key_f;
	cmp_func_t cmp_f = map->cmp_f;

	while (node) {
		struct entry *entry = rb_entry(node, struct entry, node);
		int result = cmp_f(key_f(entry_data(entry)), key);

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return entry;
	}

	return NULL;
}

void *search(struct map *map, void *key)
{
	struct entry *entry = __search(map, key);

	return entry ? entry_data(entry) : NULL;
}

static void *__remove(struct map *map, struct entry *entry)
{
	struct rb_root *root = &map->root;

	rb_erase(&entry->node, root);
	RB_CLEAR_NODE(&entry->node);
	map->size--;

	return free_entry(map, entry);
}

void *remove(struct map *map, void *key)
{
	struct entry *entry = __search(map, key);

	/* Removes entry from the tree but does not free the data */
	return entry ? __remove(map, entry) : NULL;
}

static void *__replace(struct map *map, struct entry *old, struct entry *new)
{
	struct rb_root *root = &map->root;

	rb_replace_node(&old->node, &new->node, root);

	return free_entry(map, old);
}

void *replace(struct map *map, void *data)
{
	struct entry *old, *new;

	old = __search(map, map->key_f(data));
	if (old) {
		new = alloc_entry(map, data);
		if (!new)
			return ERR_PTR(-ENOMEM);

		return __replace(map, old, new);
	}

	return ERR_PTR(-ESRCH);
}

int insert(struct map *map, void *data)
{
	struct rb_root *root = &map->root;
	struct rb_node **new = &(root->rb_node), *parent = NULL;
	key_func_t key_f = map->key_f;
	cmp_func_t cmp_f = map->cmp_f;
	void *key = key_f(data);
	struct entry *entry;

	/* Figure out where to put new node */
	while (*new) {
		struct entry *this = rb_entry(*new, struct entry, node);
		int result = cmp_f(key_f(entry_data(this)), key);

		parent = *new;
		if (result < 0)
			new = &((*new)->rb_left);
		else if (result > 0)
			new = &((*new)->rb_right);
		else /* entry already inserted */
			return -EEXIST;
	}

	entry = alloc_entry(map, data);
	if (!entry)
		return -ENOMEM;

	/* Add new node and rebalance tree. */
	rb_link_node(&entry->node, parent, new);
	rb_insert_color(&entry->node, root);
	map->size++;

	return 0;
}

int for_each_entry(struct map *map, act_func_t func, void *arg)
{
	struct rb_root *root = &map->root;
	struct rb_node *node = rb_first(root);
	int ret = 0;

	while (node) {
		struct entry *entry = rb_entry(node, struct entry, node);

		/* Stop iteration if actor returns non zero */
		ret = func(entry_data(entry), arg);
		if (ret)
			break;

		node = rb_next(node);
	}

	return ret;
}

int for_each_entry_reverse(struct map *map, act_func_t func, void *arg)
{
	struct rb_root *root = &map->root;
	struct rb_node *node = rb_last(root);
	int ret = 0;

	while (node) {
		struct entry *entry = rb_entry(node, struct entry, node);

		/* Stop iteration if actor returns non zero */
		ret = func(entry_data(entry), arg);
		if (ret)
			break;

		node = rb_prev(node);
	}

	return ret;
}

void clear(struct map *map, act_func_t destructor, void *arg)
{
	struct rb_root *root = &map->root;
	struct rb_node *node = root->rb_node;

	while (node) {
		struct entry *entry = rb_entry(node, struct entry, node);
		void *data = __remove(map, entry);

		/* call the data 'destructor' if supplied */
		if (destructor)
			destructor(data, arg);

		node = root->rb_node;
	}

	WARN(map->size, "ks_map size: %d\n", map->size);
	map->root = RB_ROOT;
}
