/**
* SSA Testing
*/


// Test function
fn tester() -> i32 {
	let x:mut u32 = 232;

	if(x == 327) {
		ret x;
	}

	x = x + 33;

	ret -1;
}



pub fn main() -> i32{
	let y:mut i32 = 23;
	let x:mut i32 = 3;
	let z:mut i32 = 322;

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

	

	ret 0;
}
