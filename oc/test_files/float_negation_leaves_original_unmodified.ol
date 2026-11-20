/**
 * Author: Jack Robbins
 * This test will verify that float negation does not clobber the original float value
 */


pub fn dummy() -> i32 {
	ret 0;
}


pub fn float_negation(x:f32) -> f32 {
	let y:f32 = -x;

	//Just so y and the negation stays around
	if(y){
		@dummy();
	}

	ret x;
}


pub fn main() -> i32 {
	let x:mut f32 = 3.0000;

	//x should *NOT* have been made negative
	x = @float_negation(x);

	OUNIT: [exit_status = 3]
	ret x;
}
