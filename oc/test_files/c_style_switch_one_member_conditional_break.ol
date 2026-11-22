/**
* Author: Jack Robbins
* Test a c-style switch statement where we have one member and a conditional break statement
* inside of our case
*/

pub fn c_style_switch_one_member(x:mut i32, y:mut i32) -> i32 {
	switch(x){
		default:
			x--;
			break;

		case 5:
			break when(y == 2);

			x++;
			break;
	}

	ret x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @c_style_switch_one_member(5, 2);
}
