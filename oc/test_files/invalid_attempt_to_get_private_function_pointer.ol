/**
* Author: Jack Robbins
* Fail case: attempt to get a pointer to a function that is in another non-parental namespace that
* is not marked as public
*/


namespace tester
{
	namespace inner
	{
		fn private_function() -> i32 {
			ret 0;
		}

	}
}


pub fn main() -> i32 {
	//Should fail, "private_function" is not visible
	let func_ptr:fn() -> i32 = tester::inner::private_function;

	ret @func_ptr();
}
