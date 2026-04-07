/**
* Author: Jack Robbins
* Test our ability to get a function pointer to a function inside of a namespace
*/


namespace sample
{
	namespace inner_sample
	{
		pub fn my_func() -> i32 {
			ret 5;
		}
	}
}



pub fn main() -> i32 {
	//This should work just fine because it's public
	let fn_ptr:fn() -> i32 = sample::inner_sample::my_func;

	//Should return 5 if all is working well
	ret @fn_ptr();
}
