/**
* Author: Jack Robbins
* Test an invalid fully qualified pointer lookup where we are looking in a namespace that is 
* *not* a predecessor
*/


namespace tester
{
	namespace inner_tester1
	{
		//Cannot see this from inside inner_tester2
		fn not_visible() -> i32 {
			ret 0;
		}

	}

	namespace inner_tester2
	{
		fn invalid_test() -> i32 {
			//This should fail, we can't see the not_visible function
			let ptr:fn() -> i32 = inner_tester1::not_visible;

			ret @ptr;
		}
	}
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
