/**
* Author: Jack Robbins
* Test an invalid case for a ternary that leads to us trying to assign from immutable to mutable
*/


pub fn ternary_assign(decider:i32, x:i32*, y:mut i32*) -> mut i32* {
	/**
	* Should fail - the type system should decide that the ternary
	* is not mutable and since the return type is, we're trying
	* to assign mutable to immutable
	*/
	ret (decider > 3) ? x else y;
}


pub fn main() -> i32 {
	ret 0;
}
