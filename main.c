#include <stdio.h>
#include <stdlib.h>

/* ---------- Node structure ---------- */
typedef struct Node {
    int key;
    int value;
    struct Node *prev;      /* for doubly linked list (recency) */
    struct Node *next;      /* for doubly linked list (recency) */
    struct Node *hashNext;  /* for chaining in hash buckets */
} Node;

/* ---------- LRU Cache structure ---------- */
typedef struct LRUCache {
    int capacity;
    int size;
    Node *head;             /* most recently used */
    Node *tail;             /* least recently used */
    int bucketCount;        /* number of hash buckets */
    Node **buckets;         /* array of bucket heads */
} LRUCache;

/* ---------- Helper functions ---------- */

/* Simple hash function: modulo bucket count */
static unsigned int hash(LRUCache *cache, int key) {
    return (unsigned int)key % cache->bucketCount;
}

/* Allocate and initialize a new Node */
static Node* createNode(int key, int value) {
    Node *n = (Node*)malloc(sizeof(Node));
    if (!n) return NULL;
    n->key = key;
    n->value = value;
    n->prev = n->next = n->hashNext = NULL;
    return n;
}

/* Add a node right after the head (becomes most recent) */
static void addToHead(LRUCache *cache, Node *node) {
    if (!cache->head) {
        cache->head = cache->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->next = cache->head;
        node->prev = NULL;
        cache->head->prev = node;
        cache->head = node;
    }
}

/* Remove a node from the doubly linked list (does not free) */
static void removeFromList(LRUCache *cache, Node *node) {
    if (node->prev)
        node->prev->next = node->next;
    else
        cache->head = node->next;

    if (node->next)
        node->next->prev = node->prev;
    else
        cache->tail = node->prev;

    node->prev = node->next = NULL;
}

/* Move an existing node to the head (most recent) */
static void moveToHead(LRUCache *cache, Node *node) {
    if (cache->head == node) return;   /* already at head */
    removeFromList(cache, node);
    addToHead(cache, node);
}

/* Evict the least recently used item (tail) */
static void evict(LRUCache *cache) {
    if (!cache->tail) return;

    Node *tail = cache->tail;

    /* Remove from hash bucket */
    int idx = hash(cache, tail->key);
    Node **bucket = &cache->buckets[idx];
    while (*bucket) {
        if (*bucket == tail) {
            *bucket = tail->hashNext;
            break;
        }
        bucket = &(*bucket)->hashNext;
    }

    /* Remove from linked list and free */
    removeFromList(cache, tail);
    free(tail);
    cache->size--;
}

/* ---------- Public interface ---------- */

LRUCache* createCache(int capacity) {
    if (capacity <= 0) return NULL;

    LRUCache *cache = (LRUCache*)malloc(sizeof(LRUCache));
    if (!cache) return NULL;

    cache->capacity = capacity;
    cache->size = 0;
    cache->head = cache->tail = NULL;

    /* Choose number of buckets as a prime > 2*capacity to reduce collisions.
       A simple prime search: start from 2*capacity+1 and find next prime. */
    int n = capacity * 2 + 1;
    while (1) {
        int isPrime = 1;
        for (int d = 2; d * d <= n; ++d) {
            if (n % d == 0) {
                isPrime = 0;
                break;
            }
        }
        if (isPrime) break;
        ++n;
    }
    cache->bucketCount = n;

    cache->buckets = (Node**)calloc(cache->bucketCount, sizeof(Node*));
    if (!cache->buckets) {
        free(cache);
        return NULL;
    }
    return cache;
}

int get(LRUCache* cache, int key) {
    if (!cache) return -1;

    int idx = hash(cache, key);
    Node *curr = cache->buckets[idx];
    while (curr) {
        if (curr->key == key) {
            moveToHead(cache, curr);
            return curr->value;
        }
        curr = curr->hashNext;
    }
    return -1;   /* not found */
}

void put(LRUCache* cache, int key, int value) {
    if (!cache) return;

    int idx = hash(cache, key);

    /* Check if key already exists */
    Node *curr = cache->buckets[idx];
    while (curr) {
        if (curr->key == key) {
            /* Update value and move to head */
            curr->value = value;
            moveToHead(cache, curr);
            return;
        }
        curr = curr->hashNext;
    }

    /* Key not present: need to insert new node */
    if (cache->size == cache->capacity) {
        evict(cache);
    }

    Node *newNode = createNode(key, value);
    if (!newNode) return;   /* allocation failure – ignore */

    /* Insert into hash bucket (at head) */
    newNode->hashNext = cache->buckets[idx];
    cache->buckets[idx] = newNode;

    /* Insert into DLL at head */
    addToHead(cache, newNode);
    cache->size++;
}

void freeCache(LRUCache* cache) {
    if (!cache) return;

    /* Free all nodes via hash buckets (each node appears exactly once) */
    for (int i = 0; i < cache->bucketCount; ++i) {
        Node *curr = cache->buckets[i];
        while (curr) {
            Node *next = curr->hashNext;
            free(curr);
            curr = next;
        }
    }
    free(cache->buckets);
    free(cache);
}

/* ---------- Utility: print cache state (from most recent to least) ---------- */
static void printCache(LRUCache *cache) {
    if (!cache) {
        printf("Cache is NULL\n");
        return;
    }
    printf("Cache (head -> tail): ");
    Node *curr = cache->head;
    while (curr) {
        printf("[%d:%d] ", curr->key, curr->value);
        curr = curr->next;
    }
    printf("\n");
}

/* ---------- Example main() ---------- */
int main(void) {
    LRUCache *cache = createCache(3);
    if (!cache) {
        fprintf(stderr, "Failed to create cache\n");
        return 1;
    }

    printf("=== LRU Cache Test (capacity=3) ===\n\n");

    put(cache, 1, 10);
    put(cache, 2, 20);
    put(cache, 3, 30);
    printf("After put(1,10), put(2,20), put(3,30):\n");
    printCache(cache);   /* expected: 3,2,1 */

    int val = get(cache, 2);
    printf("get(2) = %d\n", val);
    printf("After get(2):\n");
    printCache(cache);   /* expected: 2,3,1 */

    put(cache, 4, 40);
    printf("After put(4,40):\n");
    printCache(cache);   /* expected: 4,2,3 (evicted 1) */

    val = get(cache, 1);
    printf("get(1) = %d (should be -1)\n", val);

    val = get(cache, 3);
    printf("get(3) = %d\n", val);
    printf("After get(3):\n");
    printCache(cache);   /* expected: 3,4,2 */

    put(cache, 5, 50);
    printf("After put(5,50):\n");
    printCache(cache);   /* expected: 5,3,4 (evicted 2) */

    /* Clean up */
    freeCache(cache);
    printf("\nCache freed successfully.\n");
    return 0;
}