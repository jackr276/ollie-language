/**
* SSA Testing
*/

#file SSA_TEST1;

fn main() -> i32{
	declare x:i32;
	declare mut y:i32;

	//y := 1;
	//y := 2;
	//x := y;

	//Statically known use
	if(x > 4) then {
		y := x - 2;
	} else {
		y := x + 9;
	}

	ret 0;
}
