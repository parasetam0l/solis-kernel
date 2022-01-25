#ifndef __KS_MAP__
#define __KS_MAP__

#include <linux/rbtree.h>

typedef void *(*key_func_t)(void *);
typedef int (*cmp_func_t)(void *, void *);
typedef int (*act_func_t)(void *, void *);

struct map {
	struct rb_root root;
	int size;
	key_func_t key_f;
	cmp_func_t cmp_f;
};

#define __MAP_INITIALIZER(_key_f, _cmp_f) \
	{ \
		.root = RB_ROOT, \
		.size = 0, \
		.key_f = _key_f, \
		.cmp_f = _cmp_f \
	}

#define DEFINE_MAP(_name, _key_f, _cmp_f) \
	struct map _name = __MAP_INITIALIZER(_key_f, _cmp_f)

void *search(struct map *map, void *key);
void *remove(struct map *map, void *key);
void *replace(struct map *map, void *data);
int insert(struct map *map, void *data);
int for_each_entry(struct map *map, act_func_t func, void *arg);
int for_each_entry_reverse(struct map *map, act_func_t act, void *arg);
void clear(struct map *map, act_func_t destructor, void *arg);

#endif /* __KS_MAP__ */
