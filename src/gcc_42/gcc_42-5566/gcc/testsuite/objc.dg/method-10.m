/* When there is only one candidate method available, make sure the
   compiler uses its argument/return types when constructing the
   message sends (so that proper C/C++ argument conversions may
   take place).  */
/* { dg-do run } */

/* APPLE LOCAL radar 4894756 */
#include "../objc/execute/Object2.h"
extern void abort(void);
#define CHECK_IF(expr) if(!(expr)) abort()

static double d = 4.5920234e2;

@interface Foo : Object
-(void) brokenType: (int)x floatingPoint: (double)y;
@end


@implementation Foo
-(void) brokenType: (int)x floatingPoint: (double)y
{
	CHECK_IF(x == 459);
	CHECK_IF(y == d);
}
@end

int main(void)
{
	Foo *foo=[Foo new];
	[foo brokenType: d floatingPoint: d];
	return 0;
}

