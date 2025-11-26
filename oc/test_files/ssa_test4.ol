/**
* SSA Testing
*/

pub fn main() -> i32{
	let x:mut u32 = 33;

	x = 3222;

	for(let i:mut u32 = 3; i <= 323; ++i){
		x = 3;
		break when( x == 5);
	}

	ret 0;
}
