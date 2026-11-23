/**
 * Author: Jack Robbins
 * Attempt to branch on a casted negation value
 */


pub fn casted_negation(x:f32) -> i32 {
	if(-(<i32>x)) {
		ret 5;
	} else {
		ret 11;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 11]
	ret @casted_negation(0.0000001);
}
