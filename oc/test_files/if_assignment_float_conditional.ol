/**
 * Author: Jack Robbins
 * Test a case where we have an if-assignment that is using a float conditional. In this case, we will not
 * go through due to the extra requirements with branching(parity flag checking) that we would need
 * to do to make this a full 1-to-1 repalecement
 */

 pub fn float_if(x:f32) -> i32 {
 	declare result:mut i32;

 	if(x == 0){
		result = 2;
	} else {
		result = 3;
	}

	ret result;
 }


 pub fn main() -> i32 {
 	OUNIT: [console = 3]
 	ret @float_if(3.333);
 }
