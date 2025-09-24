/**
* SSA Testing
*/

//Global variables

fn other_test() -> void{
	let mut l:i32 = 33;
	let mut j:i32 = 3232;
let mut glob_x:u32 = 3232;
let glog_y:i32 = -232;


	if(l == 32)  {
		glob_x = 23;
		l = l + 3222;
		j = 323;
	} else {
		l = 32;
		j = l + 3;
	}

	let mut asd:i32 = l + j;
}


fn tester() -> void{
	let mut x:i32 = 33;
	let mut y:i32 = 3232;

let mut glob_x:u32 = 3232;
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
	let mut x:i32 = 33;
	let mut y:i32 = 3232;

let mut glob_x:u32 = 3232;
let glog_y:i32 = -232;
	x = 3222;
	for(let _:i32 = 2; _ < 23; _++) {
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
