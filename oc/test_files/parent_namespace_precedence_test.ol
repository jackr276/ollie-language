/**
 * Author: Jack Robbins
 * Test a case to verify that the parent namespace always takes precedence. We should
 * not be able to call a secondary namespace raw
 */


namespace my_ns1 {
	namespace ambiguous {
		pub fn my_fn() -> i32 {
			ret 5;
		}
	}
}

namespace ambiguous {
	pub fn my_fn() -> i32 {
		ret 2;
	}
}


pub fn main() -> i32 {
	OUNIT: [EXIT_STATUS = 7]
	ret @ambiguous::my_fn() + @my_ns1::ambiguous::my_fn();
}
