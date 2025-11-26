/**
* SSA Testing
*/

//Global variables

fn other_test() -> void{
	let l:mut i32 = 33;
	let j:mut i32 = 3232;
	let glob_x:mut u32 = 3232;
	let glog_y:i32 = -232;


	if(l == 32)  {
		glob_x = 23;
		l = l + 3222;
		j = 323;
	} else {
		l = 32;
		j = l + 3;
	}

	let asd:mut i32 = l + j;
}


fn tester() -> void{
	let x:mut i32 = 33;
	let y:mut i32 = 3232;

let glob_x:mut u32 = 3232;
let glog_y:i32 = -232;
	if(x <= 32) {
		x = x + 22;
	} else if(x == 23)  {
		x= 323;
	} else if(x == 36)  {
		x = 32222;
	} else {
		x = 32;
	}

	x = x + 322;
}



pub fn main() -> i32{
	let x:mut i32 = 33;
	let y:mut i32 = 3232;

let glob_x:mut u32 = 3232;
let glog_y:i32 = -232;
	x = 3222;
	for(let _:mut i32 = 2; _ < 23; _++) {
		if(x <= 32) {
			x = x + 22;
		} else {
			x = x - 3;
			y = 11;

			if( x - y == 0) {
				x = 32;
			} else {
				y = 32;
			}
		}
	}

	idle;

	ret x;
}
