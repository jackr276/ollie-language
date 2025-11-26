/**
* SSA Testing
*/

pub fn main() -> i32{
	let x:mut u32 = 33;

	if( x == 3222) {
		ret x;
	} else if (x == 11) {
		x = x + 33;
	} else {
		x--;
	}

	ret 0;
}
