/**
 * Author: Jack Robbins
 * Test the assignment of a constant(NULL which is 0) to a pointer
 */

$macro NULL <i64>0 $endmacro


pub fn tester(x:i32*) -> i32 {
	if(x == NULL){
		ret 5;
	} else {
		ret 4;
	}
}

pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @tester(NULL);
}
