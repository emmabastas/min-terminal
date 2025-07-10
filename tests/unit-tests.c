#include <stdio.h>
#include <CuTest.h>
#include "../ringbuf.h"
#include "../termbuf.h"

int main(void) {
    CuTestStart();

    CuString *output = CuStringNew();
    CuSuite *suite = CuSuiteNew();

    CuSuiteAddSuite(suite, ringbuf_test_suite());
    CuSuiteAddSuite(suite, termbuf_test_suite());
    CuSuiteRun(suite);

    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);

    CuTestEnd();

    return 0;
}
