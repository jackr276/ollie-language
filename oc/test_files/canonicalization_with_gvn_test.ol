/**
 * Author: Jack Robbins
 * Demonstrate a case where canonicalization can help the value numberer
 */


pub fn reorder(x:i32, y:i32) -> i32 {
	let a:i32 = x + y;
	let b:i32 = y + x;

	if(b == a){
		ret 5;
	} else {
		ret 7;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @reorder(11, 12);
}
