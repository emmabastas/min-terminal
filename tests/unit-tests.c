#include <stdio.h>
#include <CuTest.h>
#include "../ringbuf.h"

int main(void) {
    CuString *output = CuStringNew();
    CuSuite *suite = ringbuf_test_suite();
    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);

    return 0;
}
