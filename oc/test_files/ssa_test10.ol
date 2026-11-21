/**
* SSA Testing
*/

fn other_test(a:mut i32*) -> void{
	let l:mut i32 = 33;
	let j:mut i32 = 3232;

	if(l == 32) {
		l = l + 3222;
		j = 323;
		if(j == 323) {
			let z:mut i32 = 32;
		}
	} else {
		l = 32;
		j = l + 3;
	}

	*a = j + 2;

	ret;
}



pub fn main() -> i32{
	let x:mut i32 = 33;
	let y:mut i32 = 3232;

	if(x <= 32) {
		x = x + 22;
	} else if(x == 23) {
		x= 323;
	} else if(x == 36) {
		x = 32222;
	} else {
		x = 32;
	}

	x = x + 322;

	idle;

	ret x;
}
