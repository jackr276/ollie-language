/**
 * Author: Jack Robbins
 * Test a case where we have an if-assignment that is using a float conditional. In this case, 
 * there are additional requirements inside of the converting move instruction generator
 * to make this thing work
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
