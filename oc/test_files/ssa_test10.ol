/**
* SSA Testing
*/

fn other_test(a:mut i32*, l:mut i32, j:mut i32) -> void{
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



pub fn main(argc:i32, argv:char**) -> i32{
	let x:mut i32 = argc;
	let y:mut i32 = argc + 1;

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
