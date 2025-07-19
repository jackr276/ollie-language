/**
* SSA Testing
*/


// Test function
fn tester() -> i32 {
	let mut x:u32 := 232;

	if(x == 327) {
		ret x;
	}

	x := x + 33;

	ret -1;
}



fn main() -> i32{
	let mut y:i32 := 23;
	let mut x:i32 := 3;
	let mut z:i32 := 322;

	//Statically known use - the goal of SSA
	if(x > 4) {
		y := x - 2;
		ret y;
	} else if (x == 4) {
		y := x + 9;
	} else {
		y := x + 12;
	}
	
	//Phi function should be here
	let w:i32 := x + y;

	
	/*
	for(let i:u32 := 0; i <= 322; ++i){
		//declare w:i32;
		//w := w + 3;
	}
	*/

	while(w != x - y) {
		w++;
	}

	do {
		w++;
	} while (w != x - y);

	

	ret 0;
}
