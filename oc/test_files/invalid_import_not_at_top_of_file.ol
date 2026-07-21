/**
 * Author: Jack Robbins
 * Test a case where we see an import statement that is incorrectly placed beneath code
 */


 pub fn tester() -> i32 {
 	ret 0;
 }


//BAD - this cannot be below any code
$import "dummy";

pub fn main() -> i32 {
	OUNIT: [fail_to_compile];
	ret 0;
}
