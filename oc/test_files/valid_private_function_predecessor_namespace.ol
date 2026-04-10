/**
* Author: Jack Robbins
* Test looking up a private function inside of a predecessor namespace
*/

namespace pred
{
	//Private but visible to successors
	fn my_fn() -> i32 {
		ret 5;
	}

	namespace succ
	{
		//Should work
		pub fn adder() -> i32 {
			ret 5 + @my_fn();
		}
	}
}

pub fn main() -> i32 {
	//Should return 10
	ret @pred::succ::adder();
}
