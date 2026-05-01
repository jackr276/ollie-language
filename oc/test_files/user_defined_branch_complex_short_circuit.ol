/**
* Author: Jack Robbins
* Test our ability to make the user defined branch short circuit when appropriate
* This is a more complex version of the original short circuit optimizer
*/



pub fn user_defined_logical_and(x:mut i32, y:mut i32) -> i32 {
	let result:mut i32 = 0;

	while(true){
		result++;
		x++;
		y--;

		//Basically a break statement we're just testing
		jump end_label when(x < 15 && x > 5 && (y < 4 || y >= 9));
	}


#end_label:
	ret result;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
