#ifndef DEMOBINREADER_H
#define DEMOBINREADER_H

#include <cstddef>
#include <cstdint>

class BinReader {
public:
    BinReader(char *blob)
        : input(blob)
        , bytes_read(0){};
    uint32_t read_var_uint32();

private:
    const char *input;
    size_t bytes_read;
};

#endif // DEMOBINREADER_H
