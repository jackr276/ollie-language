/**
 * Author: Jack Robbins
 * Test a case where we are trying to redefine an already defined predeclared function
 */

declare fn predecl(i32) -> i32;


fn predecl(x:i32) -> i32 {
	ret x << 2;
}

//SHOULD FAIL
fn predecl(y:i32) -> i32 {
	ret y << 3;
}

pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
