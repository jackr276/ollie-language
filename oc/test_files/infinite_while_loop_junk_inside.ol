/**
 * Author: Jack Robbins
 * Test a case where we have an infinite while loop with an unoptimizable conditional
 */


pub fn infinite_while(x:i32) -> i32 {
	let z:i32 = 15;
	
	//WILL NEVER END
	while(x){
		//Literal useless crap
		let useless:mut i32 = 5;
		useless *= 15;
	}

	ret z * 5;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 0]
	ret 0;
}
