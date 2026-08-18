/**
 * Author: Jack Robbins
 * Test the basic use of branches in the inline
 */


pub inline fn inline_with_branches(x:i32, y:i32) -> i32 {
	let result:mut i32 = 0;

	for(let i:mut i32 = 0; i < x; i++){
		//Early return possible
		if(result == y) {
			ret result;
		}

		//Add 2 each time
		result += 2;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 4]
	ret @inline_with_branches(5, 4);
}
