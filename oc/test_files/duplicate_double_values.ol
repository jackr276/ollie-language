/**
* Author: Jack Robbins
* This file will test the compiler's ability
* to recognize and deduplicate extra local constant
* double values
*/


pub fn add_doubles() -> f64 {
	//Exact same, should deduplicate
	let x:f64 = 3.3333D;
	let y:f64 = 3.3333D;

	ret x + y;
}


pub fn main() -> i32 {
	let x:i64 = @add_doubles();
	
	if(x == 4){
		ret 1;
	} else {
		ret 0;
	}
}
