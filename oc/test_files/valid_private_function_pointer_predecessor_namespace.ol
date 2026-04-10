/**
* Author: Jack Robbins
* Test looking up a private function pointer inside of a predecessor namespace
*/

namespace pred
{
	//Private but visible to successors
	fn my_fn() -> i32 {
		ret 5;
	}

	namespace succ
	{
		pub fn adder() -> i32 {
			//Should work
			let ptr:fn() -> i32 = pred::my_fn;

			ret 5 + @ptr();
		}
	}
}

pub fn main() -> i32 {
	//Should return 10
	ret @pred::succ::adder();
}
