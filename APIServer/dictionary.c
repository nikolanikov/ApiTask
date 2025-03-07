#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "base.h"
#include "format.h"

// Items in dict with keys that hash to the same value as key
#define ITEMS_SLOT(dict, data, length) ((dict)->items + (hash((data), (length)) & ((dict)->size - 1)))

// TODO: check this sdbm hash algorithm
static uint32_t hash(const char *restrict string, size_t size)
{
	uint32_t result = 0;
	size_t i;
	for(i = 0; i < size; ++i)
		result = string[i] + (result << 6) + (result << 16) - result;
	return result;
}

/*static bool dict_resize(struct dict *restrict dict, size_t size)
{
	struct dict dict_new;
	struct dict_item **items, *item;

	if (!dict_init(&dict_new, size)) return false;

	// Get each item from each dictionary node
	while (dict->size--)
	{
		items = dict->items + dict->size;
		while (item = *items)
		{
			// Move current item from to the new dictionary
			*items = item->_next;
			item->_next = *ITEMS_SLOT(&dict_new, &item->key);
			*ITEMS_SLOT(&dict_new, &item->key) = item;
		}
	}

	free(dict->items);
	dict->items = dict_new.items;
	dict->size = size;

	return true;
}*/

// Initializes dictionary iterator
// TODO maybe rename this to dict_first
const struct dict_item *dict_first(struct dict_iterator *restrict it, const struct dict *d)
{
	for(it->index = 0; it->index < d->size; ++it->index)
		if (d->items[it->index])
			return it->item = d->items[it->index];
	return 0;
}

const struct dict_item *dict_next(struct dict_iterator *restrict it, const struct dict *d)
{
	// If there are more items in the curren slot, return the next one. Update the iterator.
	if (it->item->_next) return it->item = it->item->_next;

	for(it->index += 1; it->index < d->size; ++it->index)
		if (d->items[it->index])
			return it->item = d->items[it->index];

	return 0;
}

// TODO: is the second argument really necessary
bool dict_init(struct dict *restrict dict, size_t size)
{
	dict->items = calloc(size, sizeof(struct dict_item *));
	if (!dict->items) return false;
	dict->count = 0;
	dict->size = size;
	return true;
}

int dict_set(struct dict *restrict dict, const struct string *key, void *value, void **result)
{
	struct dict_item **items, *item;

	// Look for the requested key among the items with the corresponding hash
	// Compare only keys with the same length
	for(items = ITEMS_SLOT(dict, key->data, key->length); item = *items; items = &(*items)->_next)
		if ((key->length == item->key_size) && !memcmp(key->data, item->key_data, item->key_size))
		{
			// Overwrite value if specified. Otherwise return error.
			if (result)
			{
				*result = item->value;
				item->value = value;
				return 0;
			}
			else return ERROR_EXIST;
		}

	// Allocate a single memory slot for dict_item and key data.
	struct /* same as struct dict_item except key is not const */
	{
        size_t key_size;
		char *key_data;
        void *value;
        struct dict_item *_next;
	} *slot = malloc(sizeof(struct dict_item) + sizeof(char) * (key->length + 1));
	if (!slot) return ERROR_MEMORY;

	// Initialize the allocated memory.
	slot->key_size = key->length;
	slot->key_data = (char *)(slot + 1);
	*format_bytes(slot->key_data, key->data, key->length) = 0;
	slot->value = value;
	slot->_next = 0;

	// Add the item to the dictionary.
	*items = (struct dict_item *)slot;
	dict->count += 1;
	return 0;
}

void *dict_get(const struct dict *dict, const struct string *key)
{
	// Look for the requested key among the items with the corresponding hash
	// Compare only keys with the same length
	struct dict_item *item;
	for(item = *ITEMS_SLOT(dict, key->data, key->length); item; item = item->_next)
		if ((key->length == item->key_size) && !memcmp(key->data, item->key_data, item->key_size))
			return item->value; // This is the item we are looking for
	return 0;
}

void *dict_remove(struct dict *restrict dict, const struct string *key)
{
	struct dict_item **items, *item;

	// Look for the requested key among the items with the corresponding hash
	// Compare only keys with the same length
	for(items = ITEMS_SLOT(dict, key->data, key->length); item = *items; items = &(*items)->_next)
		if ((key->length == item->key_size) && !memcmp(key->data, item->key_data, item->key_size))
		{
			// This is the item we are looking for
			void *value = item->value;
			*items = item->_next;
			free(item);
			dict->count -= 1;
			return value;
		}

	return 0;
}

// TODO: maybe add a second argument - function pointer that can be used to free values
void dict_term(struct dict *restrict dict)
{
	struct dict_iterator it;
	struct dict_item *prev = 0;

	// Free each item in each slot of the dictionary
	for(it.index = 0; it.index < dict->size; ++it.index)
		if (dict->items[it.index])
		{
			it.item = dict->items[it.index];
			do
			{
				prev = it.item;
				it.item = it.item->_next;

				free(prev->value); // TODO: this should be done by the user
				free(prev);
			} while (it.item);
		}

	free(dict->items);
}

// TODO rename this
void dict_term_custom(struct dict *restrict dict, void (*custom)(void *))
{
	struct dict_iterator it;
	struct dict_item *prev = 0;

	// Free each item in each slot of the dictionary
	for(it.index = 0; it.index < dict->size; ++it.index)
		if (dict->items[it.index])
		{
			it.item = dict->items[it.index];
			do
			{
				prev = it.item;
				it.item = it.item->_next;

				if (custom) custom(prev->value);
				free(prev);
			} while (it.item);
		}

	free(dict->items);
}

/*#include <stdio.h>

int main(void)
{
	struct dict d, *dict;

	dict_init(&d, 4);

	struct string key, *value;

	#define sized(s) (s), sizeof(s) - 1
	#define add(k, v) do \
	{ \
		string_init(&key, sized(k)); \
		value = string_alloc(sized(v)); \
		dict_add(&d, &key, value); \
	} while (0)

	add("nikola", "nikov");
	add("assx", "qweqw");
	add("nikolff", "rrrd")

	dict = &d;

	struct dict_iterator it;
	struct dict_item *item;

	for(item = dict_first(&it, dict); item; item = dict_next(&it, dict))
		printf("%s -> %s\n", item->key.data, ((struct string *)item->value)->data);
	printf("\n");

	dict_resize(&d, 8);

	for(item = dict_first(&it, dict); item; item = dict_next(&it, dict))
		printf("%s -> %s\n", item->key.data, ((struct string *)item->value)->data);
	printf("\n");

	dict_resize(&d, 16);

	add("test", "testov");
	add("ginka", "gerberova");

	for(item = dict_first(&it, dict); item; item = dict_next(&it, dict))
		printf("%s -> %s\n", item->key.data, ((struct string *)item->value)->data);
	printf("\n");

	dict_resize(&d, 4);

	for(item = dict_first(&it, dict); item; item = dict_next(&it, dict))
		printf("%s -> %s\n", item->key.data, ((struct string *)item->value)->data);
	printf("\n");

	dict_term(&d);

	return 0;
}*/
