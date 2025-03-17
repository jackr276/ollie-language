/**
* SSA Testing
*/

#file SSA_TEST1;

fn main() -> i32{
	declare x:i32;
	declare mut y:i32;
	let mut z:i32 := 322;

	if(x > 4) then{
		y := x - 2;
		z++;
	} else {
		y := x + 9;
	}

	//Phi function here
	let w:i32 := x + y;

	ret 0;
}
