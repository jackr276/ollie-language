/**
 * Author: Jack Robbins
 * Test our ability to mutate a global variable using namespace references
 */


namespace my_namespace {
	let pub var:mut i32 = 5;
}


pub fn mutate_global() -> i32 {
	my_namespace::var = 7;
}


pub fn main() -> i32 {
	@mutate_global();

	OUNIT: [exit_status = 7]
	ret my_namespace::var;
}
