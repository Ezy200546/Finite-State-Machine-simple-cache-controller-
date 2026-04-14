#ifndef CACHE_LINE_H
#define CACHE_LINE_H

#include <string>

// Represents one line in the direct-mapped cache.
// Matches the textbook model: valid bit, dirty bit, tag, and a data block.
struct CacheLine {
    bool   valid;       // Is this line holding real data?
    bool   dirty;       // Has this line been written to (write-back needed)?
    int    tag;         // Tag bits from the address
    int    data;        // Simplified: store one integer as the "block"

    CacheLine() : valid(false), dirty(false), tag(-1), data(0) {}

    void reset() {
        valid = false;
        dirty = false;
        tag   = -1;
        data  = 0;
    }

    std::string toString() const {
        std::string s = "[";
        s += "valid=" + std::string(valid ? "1" : "0");
        s += " dirty=" + std::string(dirty ? "1" : "0");
        s += " tag=";
        if (tag == -1) s += "---";
        else {
            char buf[16];
            snprintf(buf, sizeof(buf), "0x%X", tag);
            s += buf;
        }
        s += " data=";
        if (!valid) s += "---";
        else s += std::to_string(data);
        s += "]";
        return s;
    }
};

#endif
