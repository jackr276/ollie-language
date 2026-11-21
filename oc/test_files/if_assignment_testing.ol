/**
 * Author: Jack Robbins
 * Test some complex if scenarios to see if it works properly
 */


pub fn if_assignment(x:mut i32) -> i32 {
	if(x == 3222) {
		ret x;
	} else if (x == 11) {
		x = x + 33;
	} else {
		x--;
	}

	ret x;
}


pub fn main() -> i32{
	OUNIT: [exit_status = 44]
	ret @if_assignment(11);
}
