/**
* Author: Jack Robbins
* Test an invalid fully qualified lookup where we are looking in a namespace that is 
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
		//This should fail, we can't see the not_visible function
		fn invalid_test() -> i32 {
			ret @inner_tester1::not_visible();
		}
	}
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
