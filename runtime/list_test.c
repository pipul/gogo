/* Copyright (c) 2013 Dong Fang, MIT; see COPYRIGHT */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"


struct list_node {
	int data;
	struct list_head link;
};

static int nodecnt;

struct list_node *list_node_new() {
	struct list_node *node;
	if (node = malloc(sizeof(*node))) {
		nodecnt++;
		memset(node, 0, sizeof(*node));
	}
	return node;
}

void list_node_free(struct list_node *node) {
	nodecnt--;
	free(node);
} 



int test_list() {
	int i, n = 10000;
	int sum = 0, ok;
	// the list header
	struct list_head head, newhead;
	struct list_node *node, *tmpnode;


	INIT_LIST_HEAD(&head);
	INIT_LIST_HEAD(&newhead);
	
	for (i = 0; i < n; i++) {
		node = list_node_new();
		node->data = i;
		if (!node) {
			fprintf(stderr, "-ENOMEM\n");
			break;
		}
		list_add(&node->link, &head);
	}

	list_for_each_entry(node, &head, struct list_node, link) {
		sum += node->data;
	}

	if (sum != ((0 + n - 1) * n)/2)
		goto UNPASS;

	list_for_each_entry_safe(node, tmpnode, &head, struct list_node, link) {
		if (list_empty(&newhead) || rand() % 2 == 0)
			list_move(&node->link, &newhead);
	}

	list_for_each_entry_safe(node, tmpnode, &head, struct list_node, link) {
		list_del(&node->link);
		list_node_free(node);
	}

	if (!list_empty(&head))
		goto UNPASS;

	list_for_each_entry_safe(node, tmpnode, &newhead, struct list_node, link) {
		list_move_tail(&node->link, &head);
	}

	list_for_each_entry_safe(node, tmpnode, &head, struct list_node, link) {
		if (list_empty(&newhead) || rand() % 2 == 0)
			list_move(&node->link, &newhead);
	}

	if (list_empty(&newhead) || list_empty(&head))
		goto UNPASS;

	list_splice(&head, &newhead);
	if (!list_empty(&head))
		goto UNPASS;

	list_for_each_entry_safe(node, tmpnode, &newhead, struct list_node, link) {
		list_del(&node->link);
		list_node_free(node);
	}
	
	if (nodecnt)
		goto UNPASS;

	if (!list_empty(&head) || !list_empty(&head))
		goto UNPASS;
	fprintf(stdout, "list_test ok\n");
	return 0;
 UNPASS:
	fprintf(stderr, "sum: %d\n", sum);
	abort();
}


struct mynode {
	int data;
	struct hlist_node link;
};

struct mynode *mynode_new() {
	struct mynode *node;
	if (node = malloc(sizeof(*node))) {
		nodecnt++;
		memset(node, 0, sizeof(*node));
	}
	return node;
}

void mynode_free(struct mynode *node) {
	nodecnt--;
	free(node);
} 



int test_hlist() {
	int i, n = 10000;
	int sum = 0, ok;
	// the list header
	struct hlist_head head, newhead;
	struct mynode *node, *tmpnode;


	INIT_HLIST_HEAD(&head);
	INIT_HLIST_HEAD(&newhead);
	
	for (i = 0; i < n; i++) {
		node = mynode_new();
		node->data = i;
		if (!node) {
			fprintf(stderr, "-ENOMEM\n");
			break;
		}
		hlist_add_head(&node->link, &head);
	}

	hlist_for_each_entry(node, &head, struct mynode, link) {
		sum += node->data;
	}

	if (sum != ((0 + n - 1) * n)/2)
		goto UNPASS;

	hlist_for_each_entry_safe(node, tmpnode, &head, struct mynode, link) {
		if (hlist_empty(&newhead) || rand() % 2 == 0) {
			hlist_del(&node->link);
			hlist_add_head(&node->link, &newhead);
		}
	}

	int __cnt = 0;
	hlist_for_each_entry_safe(node, tmpnode, &head, struct mynode, link) {
		__cnt += node->data;
	}
	fprintf(stdout, "%d\n", __cnt);
	
	hlist_for_each_entry_safe(node, tmpnode, &head, struct mynode, link) {
		hlist_del(&node->link);
		mynode_free(node);
	}
	__cnt = 0;
	hlist_for_each_entry_safe(node, tmpnode, &head, struct mynode, link) {
		__cnt += node->data;
	}
	fprintf(stdout, "%d\n", __cnt);

	
	if (!hlist_empty(&head))
		goto UNPASS;

	hlist_for_each_entry_safe(node, tmpnode, &newhead, struct mynode, link) {
		hlist_del(&node->link);
		if (hlist_empty(&head))
			hlist_add_head(&node->link, &head);
		else
			hlist_add_after(&node->link, head.first);
	}

	hlist_for_each_entry_safe(node, tmpnode, &head, struct mynode, link) {
		if (hlist_empty(&newhead) || rand() % 2 == 0) {
			hlist_del(&node->link);
			if (hlist_empty(&newhead))
				hlist_add_head(&node->link, &newhead);
			else
				hlist_add_before(&node->link, newhead.first);
		}
	}

	if (hlist_empty(&newhead) || hlist_empty(&head))
		goto UNPASS;

	hlist_for_each_entry_safe(node, tmpnode, &newhead, struct mynode, link) {
		hlist_del(&node->link);
		mynode_free(node);
	}

	hlist_for_each_entry_safe(node, tmpnode, &head, struct mynode, link) {
		hlist_del(&node->link);
		mynode_free(node);
	}

	
	if (nodecnt)
		goto UNPASS;

	if (!hlist_empty(&head) || !hlist_empty(&newhead))
		goto UNPASS;
	fprintf(stdout, "list_test ok\n");
	return 0;
 UNPASS:
	fprintf(stderr, "sum: %d\n", sum);
	abort();
}



int main() {
	test_list();
	test_hlist();
}
