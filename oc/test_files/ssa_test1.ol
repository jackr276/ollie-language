/**
* SSA Testing
*/


// Test function
fn tester(x:mut u32) -> i32 {
	if(x == 327) {
		ret x;
	}

	x = x + 33;

	ret -1;
}



pub fn test_fn(x:mut i32, y:mut i32, z:mut i32) -> i32 {
	//Statically known use - the goal of SSA
	if(x > 4) {
		y = x - 2;
		ret y;
	} else if (x == 4) {
		y = x + 9;
	} else {
		y = x + 12;
	}
	
	//Phi function should be here
	let w:mut i32 = x + y;

	while(w != x - y) {
		w++;
	}

	do {
		w++;
	} while (w != x - y);

	ret w;
}

//Dummy
pub fn main() -> i32{
	ret 0;
}
