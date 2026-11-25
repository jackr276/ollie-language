/**
 * Author: Jack Robbins
 * Demonstrate the effectiveness of while-changed branch elimination with a short circuiting pattern
 */


inline fn branch_on_short_circuit(x:i32, y:i32) -> i32 {
	if(x || y) {
		ret 9;
	} else {
		ret 7;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 9]
	ret @branch_on_short_circuit(0, -1);
}
