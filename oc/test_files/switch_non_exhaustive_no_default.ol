/**
 * Author: Jack Robbins
 * Test an edge case where we have a non-exhaustive switch with no default *and* no ending return
 */


 pub fn non_exhaustive(x:i32) -> i32 {
 	switch(x) {
		case 1:
			ret 5;
		case 4:
			ret 2;
		case 3:
			ret 7;
		default:
			break;
	}
 }


 pub fn main() -> i32 {
 	OUNIT: [exit_status = 2]
 	ret @non_exhaustive(4);
 }
