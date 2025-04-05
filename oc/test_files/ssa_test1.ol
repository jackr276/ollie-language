/**
* SSA Testing
*/


// Test function
fn tester() -> i32 {
	let mut x:u32 := 232;
	defer x++;

	if(x == 327) then {
		ret x;
	}

	x := x + 33;

	ret -1;
}



fn main() -> i32{
	declare x:i32;
	let mut y:i32 := 23;
	let mut z:i32 := 322;

	defer x - 3;
	defer @tester();

	//Statically known use - the goal of SSA
	if(x > 4) then{
		y := x - 2;
		ret y;
	} else if (x == 4) then {
		y := x + 9;
	} else {
		y := x + 12;
	}
	
	//Phi function should be here
	let w:i32 := x + y;

	
	/*
	for(let i:u32 := 0; i <= 322; ++i) do{
		//declare w:i32;
		//w := w + 3;
	}
	*/

	while(w != x - y) do {
		w++;
	}

	do {
		w++;
	} while (w != x - y);

	

	ret 0;
}
