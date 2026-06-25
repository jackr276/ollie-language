/**
* Author: Jack Robbins
* Test a c-style switch statement where we have one member and a conditional break statement
* inside of our case
*/


pub fn main() -> i32 {
	let x:mut i32 = 5;
	let y:mut i32 = 2;

	switch(x){
		default:
			x--;
			break;

		case 5:
			break when(y == 2);

			x++;
			break;
	}

	OUNIT: [console = 5]
	ret x;
}
