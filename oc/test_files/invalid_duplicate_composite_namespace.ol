/**
* Author: Jack Robbins
* Test a fail case where we have an invalid duplicate composite namespace that we are trying to declare
*/


namespace comp1::comp2 {
	pub fn my_fn() -> i32 {
		ret 0;
	}
}


//Should fail, comp1 already exists
namespace comp1::comp3 {
	pub fn my_fn() -> i32 {
		ret 0;
	}
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
