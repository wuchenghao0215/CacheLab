#include <stdio.h>
#include <stdlib.h>

#include "cachelab.h"


typedef struct {
    int valid;
    unsigned long tag;
    int lru;
} cache_line;

typedef struct {
    cache_line *lines;
} cache_set;

typedef struct {
    cache_set *sets;
} cache;

typedef struct {
    int s;
    int E;
    int b;
    int S;
    int B;
    int hits;
    int misses;
    int evictions;
} cache_info;

typedef struct {
    unsigned long set;
    unsigned long tag;
    unsigned long block;
} cache_addr;

typedef struct {
    char op;
    unsigned long addr;
    int size;
} operation;


unsigned long get_set(unsigned long addr, cache_info *info) {
    return (addr >> info->b) & ((1 << info->s) - 1);
}

unsigned long get_tag(unsigned long addr, cache_info *info) {
    return addr >> (info->s + info->b);
}

cache allocate_cache(cache_info *info) {
    cache c;
    c.sets = (cache_set *) malloc(sizeof(cache_set) * info->S);
    for (int i = 0; i < info->S; i++) {
        c.sets[i].lines = (cache_line *) malloc(sizeof(cache_line) * info->E);
        for (int j = 0; j < info->E; j++) {
            c.sets[i].lines[j].valid = 0;
            c.sets[i].lines[j].tag = 0;
            c.sets[i].lines[j].lru = 0;
        }
    }
    return c;
}

void free_cache(cache c, cache_info *info) {
    for (int i = 0; i < info->S; i++) {
        free(c.sets[i].lines);
    }
    free(c.sets);
}

operation parse_operation(char *line) {
    operation op;
    sscanf(line, " %c %lx,%d", &op.op, &op.addr, &op.size);
    return op;
}

cache_line *find_line(cache_set *set, cache_info *info, unsigned long tag) {
    for (int i = 0; i < info->E; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            return &set->lines[i];
        }
    }
    return NULL;
}

cache_line *find_empty_line(cache_set *set, cache_info *info) {
    for (int i = 0; i < info->E; i++) {
        if (!set->lines[i].valid) {
            return &set->lines[i];
        }
    }
    return NULL;
}

cache_line *find_lru_line(cache_set *set, cache_info *info) {
    int lru = 0;
    for (int i = 1; i < info->E; i++) {
        if (set->lines[i].lru < set->lines[lru].lru) {
            lru = i;
        }
    }
    return &set->lines[lru];
}

void update_lru(cache_set *set, cache_info *info, cache_line *line) {
    for (int i = 0; i < info->E; i++) {
        if (set->lines[i].lru < line->lru) {
            set->lines[i].lru++;
        }
    }
    line->lru = 0;
}

//void access_cache(cache *c, cache_info *info, cache_addr addr) {
//    cache_set *set = &c->sets[addr.set];
//    cache_line *line = find_line(set, info, addr.tag);
//    if (line) {
//        info->hits++;
//        update_lru(set, info, line);
//    } else {
//        info->misses++;
//        line = find_empty_line(set, info);
//        if (line) {
//            line->valid = 1;
//            line->tag = addr.tag;
//            update_lru(set, info, line);
//        } else {
//            info->evictions++;
//            line = find_lru_line(set, info);
//            line->tag = addr.tag;
//            update_lru(set, info, line);
//        }
//    }
//}

void access_cache(cache *c, cache_info *info, cache_addr addr, int verbose) {
    cache_set *set = &c->sets[addr.set];
    cache_line *line = find_line(set, info, addr.tag);
    if (line) {
        info->hits++;
        update_lru(set, info, line);
        if (verbose) {
            printf(" hit");
        }
    } else {
        info->misses++;
        line = find_empty_line(set, info);
        if (line) {
            line->valid = 1;
            line->tag = addr.tag;
            update_lru(set, info, line);
            if (verbose) {
                printf(" miss");
            }
        } else {
            info->evictions++;
            line = find_lru_line(set, info);
            line->tag = addr.tag;
            update_lru(set, info, line);
            if (verbose) {
                printf(" miss eviction");
            }
        }
    }
}

void simulate(cache *c, cache_info *info, operation op, int verbose) {
    cache_addr addr;
    addr.set = get_set(op.addr, info);
    addr.tag = get_tag(op.addr, info);
    addr.block = op.addr & ((1 << info->b) - 1);
    switch (op.op) {
        case 'L':
            if (verbose) {
                printf("L %lx,%d", op.addr, op.size);
            }
            access_cache(c, info, addr, verbose);
            if (verbose) {
                printf("\n");
            }
            break;
        case 'S':
            if (verbose) {
                printf("S %lx,%d", op.addr, op.size);
            }
            access_cache(c, info, addr, verbose);
            if (verbose) {
                printf("\n");
            }
            break;
        case 'M':
            if (verbose) {
                printf("M %lx,%d", op.addr, op.size);
            }
            access_cache(c, info, addr, verbose);
            access_cache(c, info, addr, verbose);
            if (verbose) {
                printf("\n");
            }
            break;
    }
}


int main(int argc, char **argv) {
    int verbose = 0;
    cache_info info;
    info.hits = 0;
    info.misses = 0;
    info.evictions = 0;
    char *trace_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'v':
                    verbose = 1;
                    break;
                case 's':
                    info.s = atoi(argv[++i]);
                    break;
                case 'E':
                    info.E = atoi(argv[++i]);
                    break;
                case 'b':
                    info.b = atoi(argv[++i]);
                    break;
                case 't':
                    trace_file = argv[++i];
                    break;
            }
        }
    }

    info.S = 1 << info.s;
    info.B = 1 << info.b;

    cache c = allocate_cache(&info);

    char line[100];
    FILE *fp = fopen(trace_file, "r");
    while (fgets(line, 100, fp)) {
        operation op = parse_operation(line);
        simulate(&c, &info, op, verbose);
    }

    free_cache(c, &info);

    printSummary(info.hits, info.misses, info.evictions);

    return 0;
}
