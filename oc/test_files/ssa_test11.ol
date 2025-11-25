/**
* SSA Testing
*/

pub fn main() -> i32{
	let x:mut i32 = 33;
	let y:mut i32 = 3232;

	while(x <= 32){
		x = x * 3 + x;
	}

	x = x + 3;
	ret x;
}
