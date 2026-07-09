/**
 * Author: Jack Robbins
 * Test a case where we have an if-assignment that is using a float conditional. In this case, 
 * there are additional requirements inside of the converting move instruction generator
 * to make this thing work
 */

 pub fn float_if_eq(x:f32) -> i32 {
 	declare result:mut i32;

 	if(x == 0){
		result = 2;
	} else {
		result = 3;
	}

	ret result;
 }


 pub fn float_if_not_eq(x:f32) -> i32 {
 	declare result:mut i32;

 	if(x != 3){
		result = 2;
	} else {
		result = 3;
	}

	ret result;
 }


 pub fn main() -> i32 {
 	//Should return 3 + 3 = 6
 	OUNIT: [exit_status = 6]
 	ret @float_if_eq(3.333) + @float_if_not_eq(3);
 }
