/**
* SSA Testing
*/

#file SSA_TEST1;

fn main() -> i32{
	declare x:i32;
	let mut y:i32 := 23;
	let mut z:i32 := 322;


	//Statically known use - the goal of SSA
	if(x > 4) then{
		y := x - 2;
	/*
	} else if (x == 4) then {
		y := x + 9;
	*/
	} else {
		y := x + 12;
	}
	
	//Phi function should be here
	let w:i32 := x + y;

	/*
	for(let i:u32 := 0; i <= 322; ++i) do{
	}
	*/

	ret 0;
}
