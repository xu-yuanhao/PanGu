#include "src/focused-test-support/stdio.h"
#include "src/focused-test-support/assert.h"
int main(void) {assert(printf("%s%d,%d,%d%s", "",(-2147483647-1),2147483647,0,"")==24);assert(puts("")==0);return 0;}
