/**
* Author: Jack Robbins
* We need to verify that switch assignment is never accidentally turned into conditional moves
* This test seeks to verify just that
*/


pub fn switch_assignment(x:i32) -> i32 {
	declare result:mut i32;

	switch(x){
		case 1:
			result = 2;
			break;

		case 2:
			result = 3;
			break;

		case 3:
			result = 4;
			break;

		case 4:
			result = 5;
			break;

		case 5:
			result = 6;
			break;

		case 6:
			result = 7;
			break;

		default:
			result = 11;
			break;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 4]
	ret @switch_assignment(3);
}
