/**
* SSA Testing
*/

#file SSA_TEST1;

fn main() -> i32{
	declare x:i32;
	declare mut y:i32;
	let mut z:i32 := 322;


	//Statically known use - the goal of SSA
	if(x > 4) then{
		y := x - 2;
	} else if (x == 4) then {
		y := x + 9;
	} else {
		y := x + 12;
	}

	//Phi function should be here
	let w:i32 := x + y;

	ret 0;
}
