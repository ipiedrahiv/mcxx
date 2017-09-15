/*
<testinfo>
test_generator="config/mercurium-ompss-v2"
test_compile_fail_nanos6_mercurium=yes
test_compile_fail_nanos6_imcxx=yes
</testinfo>
*/

#if !defined(__ICC) || (__ICC >= 1700)
#include <cassert>

#define epsilon 0.00025

double q = 1.0;
int num_iterations = 1000;

int main()
{
    double res = 0;

    #pragma oss task loop shared(res)
    for(int i = 0; i < num_iterations; ++i)
    {
        #pragma oss atomic
        res += q*(i + 1);
    }

    #pragma oss taskwait

    double validate = (num_iterations + 1)*num_iterations/2;
    assert(res <= validate + epsilon && res >= validate - epsilon);

    return 0;
}
#else
// ICC <17.0 seems to have precision issues with exact real representation
int main()
{
    return 0;
}
#endif
