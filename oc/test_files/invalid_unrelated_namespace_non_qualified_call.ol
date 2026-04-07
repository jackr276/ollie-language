/**
* Author: Jack Robbins
* Test an invalid case where we try to make a function call across namespaces that is *unqualified*
*/


namespace namespace1
{
	pub fn my_fn1() -> i32 {
		ret 0;
	}
}

namespace namespace2
{
	//Should not work - my_fn1() has not been fully qualified
	pub fn my_fn2() -> i32 {
		ret @my_fn1();
	}
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
