#ifndef CACHE_LINE_H
#define CACHE_LINE_H

// One line in the direct-mapped cache.
// Holds valid bit, dirty bit, tag, and data.
struct CacheLine {
    int  valid;   // 1 = holds real data, 0 = empty
    int  dirty;   // 1 = modified (not yet written to memory)
    int  tag;     // tag bits from the address
    int  data;    // the stored value
};

#endif
