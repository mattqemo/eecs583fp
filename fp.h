#ifndef _FP_H_
#define _FP_H_

#include <stdio.h>

// TODO: Optimize logging with internal DS and last-second flush to file

// struct LogLine {
//     const char* funcName;
//     const char* instName;
//     void* addr;
//     size_t size;
// };

// struct LogLineChunk {
//     struct LogLine ll[10000000];
//     struct LogLineChunk* next;
//     size_t size;
// };

// TODO: parameters for: function name, full instruction name, address, size of op
// memInstType is either 'S' for stores or 'L' for loads
void temp(size_t instID, void* addr, size_t size, char memInstType, const char* funcName) {
    static FILE* instLogFile = NULL;
    if (instLogFile == NULL) instLogFile = fopen("log.log", "w+");
    fprintf(instLogFile, "%zu\n%p\n%zu\n%c\n%s\n\n", instID, addr, size, memInstType, funcName);
}
#endif /* _FP_H_ */
