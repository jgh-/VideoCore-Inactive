//
//  util.cpp
//  Pods
//
//  Created by jinchu darwin on 15/10/8.
//
//

#include <cstdio>
#include <string>
#include "util.h"
using namespace std;

void dumpBuffer(const char *desc, uint8_t *buf, size_t size) {
    char hexBuf[size * 4 + 1];
    int j = 0;
    for (size_t i=0; i<size; i++) {
        sprintf(&hexBuf[j], "%02x ", buf[i]);
        j += 3;
        if (i % 16 == 15) {
            sprintf(&hexBuf[j], "\n");
            j++;
        }
    }
    hexBuf[j] = '\0';
    DLog("%s:\n%s\n", desc, hexBuf);
}