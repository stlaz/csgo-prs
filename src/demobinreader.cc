#include <iostream>

#include "demobinreader.h"

uint32_t BinReader::read_var_uint32()
{
    /*
     * Reads up to 5 bytes to get a 32-bit unsigned int value. In each byte,
     * there are 7 data bits and 1 control bit which indicates whether
     * to read on or not.
     */
    uint32_t ret = 0;
    int read_times = 0;

    for (int read_times = 0; read_times < 5; read_times++) {
        std::cout << ">> BYTES READ: " << bytes_read << std::endl;
        std::cout << ">> TO ADD: " << ((int)input[bytes_read] & 0x7F)
                  << std::endl;
        ret |= (input[bytes_read] & 0x7F) << (7 * read_times);
        std::cout << ">> INTERMEDIATE RESULT: " << ret << std::endl;
        if ((input[bytes_read++] & 0x80) == 0) {
            break;
        }
    }
    return ret;
}
